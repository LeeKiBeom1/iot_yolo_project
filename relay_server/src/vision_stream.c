#include "vision_stream.h"
#include "vision_io.h"
#include "json.h"

/* One session/owner: reconnect never replays an old pending frame. */
static void gateway_session(int upstream, const char *ip, int port)
{
    char queue[VISION_QUEUE][VISION_LIMIT + 1], control[VISION_LIMIT + 1];
    int downstream = -1, ready = 0, head = 0, count = 0;
    int64_t retry_at = 0, state_deadline = 0, report_at = 0;
    unsigned long sequence = 0, received = 0, forwarded = 0, dropped = 0;
    vision_socket(upstream);
    if (vision_control(upstream, "gateway-pi-01", 0,
                       "server_state_pending", &sequence) < 0) return;
    for (;;) {
        int64_t now = vision_clock(CLOCK_MONOTONIC);
        struct pollfd fds[2];
        if (downstream < 0 && now >= retry_at) {
            downstream = vision_connect(ip, port);
            retry_at = vision_clock(CLOCK_MONOTONIC) + 1000;
            state_deadline = vision_clock(CLOCK_MONOTONIC) + 5000;
        }
        fds[0] = (struct pollfd){upstream, POLLIN, 0};
        fds[1] = (struct pollfd){downstream, POLLIN, 0};
        if (poll(fds, 2, count ? 0 : 100) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (fds[1].revents) {
            int state;
            if (vision_read(downstream, control) < 0 ||
                (state = vision_parse_control(control)) < 0) goto lost;
            ready = state;
            state_deadline = 0;
            if (!ready) { dropped += count; count = 0; head = 0; }
            if (vision_write(upstream, control) < 0) break;
        }
        if (downstream >= 0 && state_deadline &&
            vision_clock(CLOCK_MONOTONIC) >= state_deadline) goto lost;
        if (fds[0].revents) {
            char json[VISION_LIMIT + 1];
            VisionMessage message;
            if (vision_read(upstream, json) < 0) break;
            if (parse_vision_json(json, &message) < 0) {
                fprintf(stderr, "vision invalid payload; closing session\n");
                break;
            }
            received++;
            if (!ready) dropped++;
            else {
                if (count == VISION_QUEUE) {
                    head = (head + 1) % VISION_QUEUE; count--; dropped++;
                }
                strcpy(queue[(head + count) % VISION_QUEUE], json);
                count++;
            }
        }
        if (ready && count) {
            if (vision_write(downstream, queue[head]) < 0) goto lost;
            head = (head + 1) % VISION_QUEUE; count--; forwarded++;
        }
        if (now >= report_at) {
            fprintf(stderr, "vision gateway received=%lu forwarded=%lu dropped=%lu queue=%d ready=%d\n",
                    received, forwarded, dropped, count, ready);
            report_at = now + 5000;
        }
        continue;
lost:
        if (downstream >= 0) close(downstream);
        downstream = -1; ready = 0; dropped += count; count = 0; head = 0;
        retry_at = vision_clock(CLOCK_MONOTONIC) + 1000;
        if (vision_control(upstream, "gateway-pi-01", 0,
                           "final_connection_lost", &sequence) < 0) break;
    }
    if (downstream >= 0) close(downstream);
    fprintf(stderr, "vision gateway session ended pending_discarded=%d\n", count);
}

void *vision_service(void *unused)
{
    int listener, client;
    const char *ip = getenv("UBUNTU_SERVER_IP");
    int port = vision_port("VISION_PORT", 5002);
    int final_port = vision_port("VISION_FINAL_PORT", 5003);
    (void)unused;
    if (!ip) ip = "10.10.16.51";
    listener = vision_listen(port);
    if (listener < 0) { perror("Vision listener"); return NULL; }
    fprintf(stderr, "Vision gateway :%d -> %s:%d (no ACK)\n", port, ip, final_port);
    for (;;) {
        client = accept(listener, NULL, NULL);
        if (client < 0) { if (errno == EINTR) continue; break; }
        gateway_session(client, ip, final_port);
        close(client);
    }
    close(listener);
    return NULL;
}
