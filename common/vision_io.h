#ifndef VISION_IO_H
#define VISION_IO_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <cjson/cJSON.h>

#define VISION_LIMIT 4096
#define VISION_QUEUE 64

static inline int64_t vision_clock(clockid_t clock)
{
    struct timespec t;
    clock_gettime(clock, &t);
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static inline int vision_port(const char *name, int fallback)
{
    const char *s = getenv(name);
    char *end;
    long n;
    if (!s) return fallback;
    n = strtol(s, &end, 10);
    return *s && !*end && n > 0 && n <= 65535 ? (int)n : -1;
}

static inline void vision_socket(int fd)
{
    int one = 1, idle = 10, interval = 3, count = 3, timeout = 20000;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
    setsockopt(fd, IPPROTO_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout));
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

/* TCP read/write 한 번에 전체 메시지가 처리된다고 보장할 수 없다.
   처리한 바이트 수를 더하며 나머지를 반복한다. 헤더와 본문은 하나의 1초 마감을 공유해
   일부 바이트만 계속 보내는 상대 때문에 무한정 기다리지 않는다. */
static inline int vision_io(int fd, void *data, size_t size, int writing,
                            int64_t deadline)
{
    size_t done = 0;
    while (done < size) {
        int64_t remaining = deadline - vision_clock(CLOCK_MONOTONIC);
        struct pollfd p = {fd, writing ? POLLOUT : POLLIN, 0};
        ssize_t n;
        int r;
        if (remaining <= 0) return -1;
        r = poll(&p, 1, (int)remaining);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        n = writing ? send(fd, (char *)data + done, size - done, MSG_NOSIGNAL)
                    : recv(fd, (char *)data + done, size - done, 0);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static inline int vision_write(int fd, const char *json)
{
    size_t n = strlen(json);
    uint32_t header = htonl((uint32_t)n);
    int64_t deadline = vision_clock(CLOCK_MONOTONIC) + 1000;
    if (!n || n > VISION_LIMIT) return -1;
    if (vision_io(fd, &header, 4, 1, deadline) < 0) return -1;
    return vision_io(fd, (void *)json, n, 1, deadline);
}

static inline int vision_read(int fd, char *json)
{
    /* 메모리 보호를 위해 길이부터 검사한다. 호출자의 버퍼는 VISION_LIMIT+1이어야 한다.
       4바이트 길이에는 헤더 자신이 아니라 UTF-8 JSON 본문 바이트 수만 들어간다. */
    uint32_t header, n;
    int64_t deadline = vision_clock(CLOCK_MONOTONIC) + 1000;
    if (vision_io(fd, &header, 4, 0, deadline) < 0) return -1;
    n = ntohl(header);
    if (!n || n > VISION_LIMIT) return -1;
    if (vision_io(fd, json, n, 0, deadline) < 0) return -1;
    if (memchr(json, '\0', n)) return -1;
    json[n] = '\0';
    return 0;
}

static inline int vision_listen(int port)
{
    int fd, one = 1;
    struct sockaddr_in addr = {0};
    if (port < 1) return -1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (void *)&addr, sizeof(addr)) < 0 || listen(fd, 4) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static inline int vision_connect(const char *ip, int port)
{
    struct sockaddr_in addr = {0};
    struct pollfd p;
    int fd, error = 0;
    socklen_t size = sizeof(error);
    if (port < 1) return -1;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    vision_socket(fd);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) goto fail;
    if (connect(fd, (void *)&addr, sizeof(addr)) == 0) return fd;
    if (errno != EINPROGRESS) goto fail;
    p = (struct pollfd){fd, POLLOUT, 0};
    if (poll(&p, 1, 1000) <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error) goto fail;
    return fd;
fail:
    close(fd);
    return -1;
}

/* 소켓을 담당하는 스레드 하나에서만 Control을 전송한다.
   DB 준비 상태를 pause/resume으로 전달하며 객체별 저장 ACK를 대신하는 것은 아니다. */
static inline int vision_control(int fd, const char *device, int ready,
                                 const char *reason, unsigned long *sequence)
{
    char json[512];
    long long now = (long long)vision_clock(CLOCK_REALTIME);
    snprintf(json, sizeof(json),
        "{\"version\":1,\"type\":\"control\",\"device_id\":\"%s\","
        "\"message_id\":\"%s-%lld-%ld-%lu\",\"data\":{\"timestamp_ms\":%lld,"
        "\"action\":\"%s\",\"reason\":\"%s\"}}",
        device, device, now, (long)getpid(), ++*sequence, now,
        ready ? "resume" : "pause", reason);
    fprintf(stderr, "vision control %s %s\n", ready ? "resume" : "pause", reason);
    return vision_write(fd, json);
}

static inline int vision_parse_control(const char *json)
{
    const char *end = NULL;
    cJSON *r = cJSON_ParseWithOpts(json, &end, 1);
    cJSON *v = cJSON_GetObjectItemCaseSensitive(r, "version");
    cJSON *t = cJSON_GetObjectItemCaseSensitive(r, "type");
    cJSON *device = cJSON_GetObjectItemCaseSensitive(r, "device_id");
    cJSON *id = cJSON_GetObjectItemCaseSensitive(r, "message_id");
    cJSON *d = cJSON_GetObjectItemCaseSensitive(r, "data");
    cJSON *a = cJSON_GetObjectItemCaseSensitive(d, "action");
    cJSON *reason = cJSON_GetObjectItemCaseSensitive(d, "reason");
    cJSON *ts = cJSON_GetObjectItemCaseSensitive(d, "timestamp_ms");
    int ready = -1;
    if (cJSON_IsNumber(v) && v->valuedouble == 1 &&
        cJSON_IsString(t) && !strcmp(t->valuestring, "control") &&
        cJSON_IsString(device) && *device->valuestring &&
        cJSON_IsString(id) && *id->valuestring &&
        cJSON_IsNumber(ts) && ts->valuedouble > 0 &&
        cJSON_IsString(reason) && *reason->valuestring && cJSON_IsString(a)) {
        if (!strcmp(a->valuestring, "pause")) ready = 0;
        if (!strcmp(a->valuestring, "resume")) ready = 1;
    }
    cJSON_Delete(r);
    return ready;
}
#endif
