#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "database.h"
#include "json.h"
#include "server.h"

#define PORT 5001

static void handle_client(int clnt_sock, MYSQL *database)
{
    char buf[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];
    char type[16];
    SensorMessage sensor_message;
    VisionMessage message;
    int receive_result;
    int save_result;

    while (1) {
        receive_result = recv_frame(clnt_sock, buf, sizeof(buf));
        if (receive_result == 0) {
            break;
        }
        if (receive_result < 0) {
            fprintf(stderr, "Failed to receive frame\n");
            break;
        }

        printf("Received: %s\n", buf);

        if (parse_message_type(buf, type, sizeof(type)) < 0) {
            fprintf(stderr, "Invalid JSON message\n");
            break;
        }

        if (strcmp(type, "sensor") == 0) {
            if (parse_sensor_json(buf, &sensor_message) < 0) {
                fprintf(stderr, "Invalid sensor message\n");
                break;
            }
            save_result = database_save_sensor(database, &sensor_message);
        } else if (strcmp(type, "vision") == 0) {
            if (parse_vision_json(buf, &message) < 0) {
                fprintf(stderr, "Invalid vision message\n");
                break;
            }
            save_result = database_save_vision(database, &message);
        } else {
            fprintf(stderr, "Unsupported message type\n");
            break;
        }

        if (create_ack_json(strcmp(type, "sensor") == 0
                                ? sensor_message.message_id
                                : message.message_id,
                            save_result == DB_SAVE_OK ||
                            save_result == DB_SAVE_DUPLICATE ? "ok" : "error",
                            save_result == DB_SAVE_DUPLICATE,
                            save_result == DB_SAVE_CONFLICT
                                ? "MESSAGE_ID_CONFLICT"
                                : save_result == DB_SAVE_ERROR
                                    ? "DATABASE_ERROR" : NULL,
                            ack, sizeof(ack)) < 0) {
            fprintf(stderr, "Failed to create ACK JSON\n");
            break;
        }

        if (send_frame(clnt_sock, ack) < 0) {
            fprintf(stderr, "Failed to send ACK frame\n");
            break;
        }
    }
}

int main(void)
{
    int serv_sock, clnt_sock;
    int reuse_address = 1;
    struct sockaddr_in serv_addr;
    MYSQL *database;

    database = database_connect();
    if (database == NULL) {
        return 1;
    }

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serv_sock < 0) {
        perror("socket");
        database_close(database);
        return 1;
    }

    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_address, sizeof(reuse_address)) < 0) {
        perror("setsockopt");
        close(serv_sock);
        database_close(database);
        return 1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(serv_sock);
        database_close(database);
        return 1;
    }

    if (listen(serv_sock, 5) < 0) {
        perror("listen");
        close(serv_sock);
        database_close(database);
        return 1;
    }

    printf("Ubuntu Server waiting on port %d...\n", PORT);

    while (1) {
        clnt_sock = accept(serv_sock, NULL, NULL);
        if (clnt_sock < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        handle_client(clnt_sock, database);
        close(clnt_sock);
    }

    close(serv_sock);
    database_close(database);

    return 1;
}
