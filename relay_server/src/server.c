#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

static int send_all(int socket_fd, const void *buffer, int length)
{
    const char *data = buffer;
    int sent = 0;

    while (sent < length) {
        ssize_t result = send(socket_fd, data + sent, length - sent, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (result == 0) {
            return -1;
        }

        sent += (int)result;
    }

    return 0;
}

static int recv_all(int socket_fd, void *buffer, int length)
{
    char *data = buffer;
    int received = 0;

    while (received < length) {
        ssize_t result = recv(socket_fd, data + received, length - received, 0);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (result == 0) {
            return 0;
        }

        received += (int)result;
    }

    return 1;
}

int send_frame(int socket_fd, const char *json)
{
    uint32_t payload_size;
    uint32_t network_size;

    if (json == NULL) {
        return -1;
    }

    payload_size = (uint32_t)strlen(json);
    if (payload_size == 0 || payload_size > MAX_MESSAGE_SIZE) {
        return -1;
    }

    network_size = htonl(payload_size);

    if (send_all(socket_fd, &network_size, sizeof(network_size)) < 0) {
        return -1;
    }

    return send_all(socket_fd, json, (int)payload_size);
}

int recv_frame(int socket_fd, char *buffer, int buffer_size)
{
    uint32_t network_size;
    uint32_t payload_size;
    int result;

    if (buffer == NULL || buffer_size <= 1) {
        return -1;
    }

    result = recv_all(socket_fd, &network_size, sizeof(network_size));
    if (result <= 0) {
        return result;
    }

    payload_size = ntohl(network_size);
    if (payload_size == 0 ||
        payload_size > MAX_MESSAGE_SIZE ||
        payload_size >= (uint32_t)buffer_size) {
        return -1;
    }

    result = recv_all(socket_fd, buffer, (int)payload_size);
    if (result <= 0) {
        return -1;
    }

    buffer[payload_size] = '\0';
    return (int)payload_size;
}
