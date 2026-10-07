#include "vision_stream.h"
#include "vision_io.h"
#include "database.h"
#include <pthread.h>

typedef struct {
    int is_count;
    union {
        VisionMessage vision;
        VehicleCountMessage count;
    } data;
} StreamMessage;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    StreamMessage queue[VISION_QUEUE];
    int head, count, stop, ready;
    unsigned long received, saved, dropped;
    unsigned long vision_received, count_received;
    unsigned long vision_saved, count_saved;
    VehicleCountMessage latest_count;
    int has_latest_count;
    long long latency_ms;
} VisionSession;

/* DB 작업 스레드만 이 MariaDB 연결을 사용한다. 소켓 스레드는 수신과 Control에 집중한다.
   mutex는 큐/공유 상태를 보호하며 SQL 실행 중에는 풀어 수신을 막지 않는다.
   센서는 별도의 MariaDB 연결을 사용하므로 두 경로의 연결 객체를 공유하지 않는다. */
static void *vision_writer(void *arg)
{
    VisionSession *s = arg;
    MYSQL *db = NULL;
    mysql_thread_init();
    for (;;) {
        StreamMessage message;
        int result;
        int64_t start;
        pthread_mutex_lock(&s->mutex);
        if (s->stop) { pthread_mutex_unlock(&s->mutex); break; }
        pthread_mutex_unlock(&s->mutex);
        if (!db) {
            db = database_connect();
            if (db && mysql_query(db, "SELECT v.id FROM vision_data v JOIN vehicle_count c ON 1=0 LIMIT 0") != 0) {
                database_close(db); db = NULL;
            } else if (db) {
                MYSQL_RES *result = mysql_store_result(db);
                if (result) mysql_free_result(result);
            }
            if (!db) {
                struct timespec delay = {1, 0};
                nanosleep(&delay, NULL);
                continue;
            }
            pthread_mutex_lock(&s->mutex);
            s->ready = 1;
            pthread_mutex_unlock(&s->mutex);
        }
        pthread_mutex_lock(&s->mutex);
        while (!s->stop && !s->count) {
            struct timespec until;
            clock_gettime(CLOCK_MONOTONIC, &until); until.tv_sec++;
            if (pthread_cond_timedwait(&s->changed, &s->mutex, &until) == ETIMEDOUT) break;
        }
        if (s->stop) { pthread_mutex_unlock(&s->mutex); break; }
        if (!s->count) {
            pthread_mutex_unlock(&s->mutex);
            if (mysql_ping(db) == 0) continue;
            goto failed;
        }
        message = s->queue[s->head];
        s->head = (s->head + 1) % VISION_QUEUE; s->count--;
        pthread_mutex_unlock(&s->mutex);
        start = vision_clock(CLOCK_MONOTONIC);
        result = message.is_count ?
            database_save_vehicle_count(db, &message.data.count) :
            database_save_vision(db, &message.data.vision);
        pthread_mutex_lock(&s->mutex);
        s->latency_ms = vision_clock(CLOCK_MONOTONIC) - start;
        if (result == DB_SAVE_OK) {
            s->saved++;
            if (message.is_count) s->count_saved++;
            else s->vision_saved++;
        }
        else if (result == DB_SAVE_CONFLICT) {
            s->dropped++;
        }
        pthread_mutex_unlock(&s->mutex);
        if (result == DB_SAVE_CONFLICT)
            fprintf(stderr, "stream MESSAGE_ID_CONFLICT id=%s\n",
                    message.is_count ? message.data.count.message_id : message.data.vision.message_id);
        if (result != DB_SAVE_ERROR) continue;
        pthread_mutex_lock(&s->mutex);
        s->dropped++; /* Failed in-flight item is not replayed. */
        pthread_mutex_unlock(&s->mutex);
failed:
        pthread_mutex_lock(&s->mutex);
        s->ready = 0;
        s->dropped += s->count;
        s->count = 0; s->head = 0;
        pthread_mutex_unlock(&s->mutex);
        fprintf(stderr, "vision database_write_failed time_ms=%lld\n",
                (long long)vision_clock(CLOCK_REALTIME));
        database_close(db); db = NULL;
        { struct timespec delay = {1, 0}; nanosleep(&delay, NULL); }
    }
    database_close(db);
    mysql_thread_end();
    return NULL;
}

/* 소켓 소유자는 이 함수 하나다. ready 변화 때만 Control을 보내 로그/통신 도배를 줄인다.
   큐가 가득 차거나 DB가 끊기면 pause하고 남은 데이터는 폐기한다.
   객체별 ACK/재전송을 보장하는 센서 경로와 혼동하지 않도록 dropped를 기록한다. */
static void final_session(int fd)
{
    VisionSession s = {0};
    pthread_t worker;
    pthread_condattr_t condition_attributes;
    unsigned long sequence = 0;
    unsigned long reported_received = 0;
    unsigned long reported_vision = 0, reported_counts = 0;
    unsigned long reported_vision_saved = 0, reported_count_saved = 0;
    unsigned long reported_dropped = 0;
    int last_ready = -1, overload = 0;
    int64_t report_at = vision_clock(CLOCK_MONOTONIC) + 5000;
    int64_t overloaded_at = 0;
    pthread_mutex_init(&s.mutex, NULL);
    pthread_condattr_init(&condition_attributes);
    pthread_condattr_setclock(&condition_attributes, CLOCK_MONOTONIC);
    pthread_cond_init(&s.changed, &condition_attributes);
    pthread_condattr_destroy(&condition_attributes);
    vision_socket(fd);
    if (pthread_create(&worker, NULL, vision_writer, &s) != 0) goto cleanup;
    for (;;) {
        int ready, depth;
        struct pollfd p = {fd, POLLIN, 0};
        int64_t now = vision_clock(CLOCK_MONOTONIC);
        pthread_mutex_lock(&s.mutex);
        depth = s.count;
        if (overload && s.ready && !depth && now - overloaded_at >= 1000) overload = 0;
        ready = s.ready && !overload;
        if (now >= report_at) {
            unsigned long received = s.received, dropped = s.dropped;
            unsigned long vision_received = s.vision_received;
            unsigned long count_received = s.count_received;
            unsigned long vision_saved = s.vision_saved;
            unsigned long count_saved = s.count_saved;
            VehicleCountMessage latest_count = s.latest_count;
            int has_latest_count = s.has_latest_count;
            long long latency = s.latency_ms;
            pthread_mutex_unlock(&s.mutex);
            if (received != reported_received) {
                fprintf(stderr,
                    "data 5s vision_rx=%lu vehicle_count_rx=%lu "
                    "vision_saved=%lu vehicle_count_saved=%lu dropped=%lu "
                    "queue=%d db_ms=%lld ready=%d",
                    vision_received - reported_vision,
                    count_received - reported_counts,
                    vision_saved - reported_vision_saved,
                    count_saved - reported_count_saved,
                    dropped - reported_dropped, depth, latency, ready);
                if (has_latest_count && count_received != reported_counts) {
                    fprintf(stderr,
                        " latest_vehicle_count=%d frame_id=%llu timestamp_ms=%lld",
                        latest_count.vehicle_count,
                        (unsigned long long)latest_count.frame_id,
                        (long long)latest_count.timestamp_ms);
                }
                fputc('\n', stderr);
            }
            reported_received = received;
            reported_vision = vision_received;
            reported_counts = count_received;
            reported_vision_saved = vision_saved;
            reported_count_saved = count_saved;
            reported_dropped = dropped;
            report_at = now + 5000;
        } else pthread_mutex_unlock(&s.mutex);
        if (ready != last_ready) {
            if (vision_control(fd, "final-server-01", ready,
                ready ? "server_ready" : overload ? "queue_overload" : "database_unavailable",
                &sequence) < 0) break;
            last_ready = ready;
        }
        if (poll(&p, 1, 100) < 0) { if (errno == EINTR) continue; break; }
        if (p.revents) {
            char json[VISION_LIMIT + 1];
            StreamMessage message = {0};
            char type[32];
            if (vision_read(fd, json) < 0 ||
                parse_message_type(json, type, sizeof(type)) < 0) break;
            if (strcmp(type, "vehicle_count") == 0) {
                message.is_count = 1;
                if (parse_vehicle_count_json(json, &message.data.count) < 0) break;
            } else if (strcmp(type, "vision") != 0 ||
                       parse_vision_json(json, &message.data.vision) < 0) break;
            pthread_mutex_lock(&s.mutex);
            s.received++;
            if (message.is_count) {
                s.count_received++;
                s.latest_count = message.data.count;
                s.has_latest_count = 1;
            } else {
                s.vision_received++;
            }
            if (!s.ready || overload) s.dropped++;
            else if (s.count == VISION_QUEUE) {
                s.dropped += s.count + 1;
                s.count = 0; s.head = 0;
                overload = 1; overloaded_at = vision_clock(CLOCK_MONOTONIC);
            } else {
                s.queue[(s.head + s.count) % VISION_QUEUE] = message;
                s.count++;
                pthread_cond_signal(&s.changed);
            }
            pthread_mutex_unlock(&s.mutex);
        }
    }
    pthread_mutex_lock(&s.mutex);
    s.stop = 1; s.dropped += s.count; s.count = 0;
    pthread_cond_signal(&s.changed);
    pthread_mutex_unlock(&s.mutex);
    pthread_join(worker, NULL);
cleanup:
    pthread_cond_destroy(&s.changed);
    pthread_mutex_destroy(&s.mutex);
}

void *vision_service(void *unused)
{
    int port = vision_port("VISION_PORT", 5003);
    int listener = vision_listen(port), fd;
    (void)unused;
    if (listener < 0) { perror("Vision listener"); return NULL; }
    fprintf(stderr, "Vision final :%d (no ACK)\n", port);
    for (;;) {
        fd = accept(listener, NULL, NULL);
        if (fd < 0) { if (errno == EINTR) continue; break; }
        final_session(fd);
        close(fd);
    }
    close(listener);
    return NULL;
}
