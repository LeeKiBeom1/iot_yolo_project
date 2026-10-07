#include "sync.h"

#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>

#include "database.h"
#include "json.h"
#include "server.h"
#include "vision_io.h"

/* 센서 동기화 중에는 SQLite 잠금을 가지고 있으므로 무한 대기를 피한다.
   connect는 공통 함수로 최대 1초 대기하고, 연결 후 송수신은 3초 제한을 둔다.
   실패한 행은 UNSENT로 남아 다음 측정 수신 또는 서버 재시작 때 다시 보낸다. */
static int connect_sync(const char *ip, int port)
{
    int fd = vision_connect(ip, port);
    struct timeval timeout = {3, 0};
    int flags;
    if (fd < 0) return -1;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int sync_unsent_sensors(sqlite3 *database,
                        const char *server_ip, int server_port)
{
    int socket_fd;
    int row_result;
    SensorMessage message;
    char json[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];

    row_result = database_get_unsent_sensor(database, &message);
    if (row_result <= 0) return row_result;

    socket_fd = connect_sync(server_ip, server_port);
    if (socket_fd < 0) return -1;

    /* ACK의 ID와 성공 상태를 확인한 행만 SENT로 바꾼다.
       저장 성공 후 ACK만 유실되어도 같은 ID를 재전송하므로 DB에서 중복을 판별한다. */
    do {
        if (create_sensor_json(&message, json, sizeof(json)) < 0 ||
            send_frame(socket_fd, json) < 0 ||
            recv_frame(socket_fd, ack, sizeof(ack)) <= 0 ||
            validate_ack_json(ack, message.message_id) < 0 ||
            database_mark_sensor_sent(database, message.message_id) < 0) {
            close(socket_fd);
            return -1;
        }
        row_result = database_get_unsent_sensor(database, &message);
    } while (row_result > 0);

    close(socket_fd);
    return row_result;
}
int sync_unsent_vision(sqlite3 *database,
                       const char *server_ip, int server_port)
{
    int socket_fd;
    int row_result;
    VisionMessage message;
    char json[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];

    row_result = database_get_unsent_vision(database, &message);
    if (row_result <= 0) return row_result;

    socket_fd = connect_sync(server_ip, server_port);
    if (socket_fd < 0) return -1;

    do {
        if (create_vision_json(&message, json, sizeof(json)) < 0 ||
            send_frame(socket_fd, json) < 0 ||
            recv_frame(socket_fd, ack, sizeof(ack)) <= 0 ||
            validate_ack_json(ack, message.message_id) < 0 ||
            database_mark_vision_sent(database, message.message_id) < 0) {
            close(socket_fd);
            return -1;
        }
        row_result = database_get_unsent_vision(database, &message);
    } while (row_result > 0);

    close(socket_fd);
    return row_result;
}
