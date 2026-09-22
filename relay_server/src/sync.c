#include "sync.h"

#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "database.h"
#include "json.h"
#include "server.h"

int sync_unsent_sensors(sqlite3 *database,
                        const char *server_ip, int server_port)
{
    int socket_fd;
    int row_result;
    struct sockaddr_in server_address;
    SensorMessage message;
    char json[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];

    row_result = database_get_unsent_sensor(database, &message);
    if (row_result <= 0) return row_result;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) != 1 ||
        connect(socket_fd, (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0) {
        close(socket_fd);
        return -1;
    }

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
    struct sockaddr_in server_address;
    VisionMessage message;
    char json[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];

    row_result = database_get_unsent_vision(database, &message);
    if (row_result <= 0) return row_result;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) != 1 ||
        connect(socket_fd, (struct sockaddr *)&server_address,
                sizeof(server_address)) < 0) {
        close(socket_fd);
        return -1;
    }

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
