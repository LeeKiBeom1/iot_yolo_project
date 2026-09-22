#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "json.h"
#include "server.h"

#define SERVER_IP "10.10.16.51"
#define PORT 5001

int main(void)
{
    int sock;
    struct sockaddr_in serv_addr;
    char buf[MAX_MESSAGE_SIZE + 1];
    const char *message_id = "relay-test-000001-00000001";
    const char *json =
        "{\"version\":1,\"type\":\"sensor\","
        "\"device_id\":\"relay-test\","
        "\"message_id\":\"relay-test-000001-00000001\","
        "\"timestamp\":\"2026-09-22T00:00:00\","
        "\"data\":{\"light\":0,\"temperature\":0,"
        "\"humidity\":0,\"sound\":0}}";

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server IP\n");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    if (send_frame(sock, json) < 0) {
        fprintf(stderr, "Failed to send frame\n");
        close(sock);
        return 1;
    }

    if (recv_frame(sock, buf, sizeof(buf)) <= 0) {
        fprintf(stderr, "Failed to receive ACK frame\n");
        close(sock);
        return 1;
    }

    if (validate_ack_json(buf, message_id) < 0) {
        fprintf(stderr, "Invalid ACK: %s\n", buf);
        close(sock);
        return 1;
    }

    printf("ACK: %s\n", buf);

    close(sock);

    return 0;
}
