#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "database.h"
#include "json.h"
#include "server.h"
#include "sync.h"

#define PORT 5000
#define UBUNTU_PORT 5001
#define DATABASE_PATH "db/road_monitor.db"

static void handle_client(int client_socket, sqlite3 *database,
                          const char *ubuntu_ip)
{
    char buffer[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];
    SensorMessage message;
    int receive_result;
    int save_result;

    while (1) {
        receive_result = recv_frame(client_socket, buffer, sizeof(buffer));
        if (receive_result == 0) break;
        if (receive_result < 0 || parse_sensor_json(buffer, &message) < 0) {
            fprintf(stderr, "Invalid sensor message\n");
            break;
        }

        save_result = database_save_sensor(database, &message);
        if (create_ack_json(message.message_id,
                            save_result == DB_SAVE_ERROR ? "error" : "ok",
                            save_result == DB_SAVE_DUPLICATE,
                            save_result == DB_SAVE_ERROR
                                ? "DATABASE_ERROR" : NULL,
                            ack, sizeof(ack)) < 0 ||
            send_frame(client_socket, ack) < 0) {
            break;
        }

        if (save_result != DB_SAVE_ERROR &&
            sync_unsent_sensors(database, ubuntu_ip, UBUNTU_PORT) < 0) {
            fprintf(stderr, "Ubuntu sync deferred\n");
        }
    }
}

int main(void)
{
    int server_socket;
    int client_socket;
    int reuse_address = 1;
    struct sockaddr_in server_address;
    sqlite3 *database;
    const char *ubuntu_ip = getenv("UBUNTU_SERVER_IP");

    if (ubuntu_ip == NULL) ubuntu_ip = "10.10.16.51";
    database = database_open(DATABASE_PATH);
    if (database == NULL) return 1;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        database_close(database);
        return 1;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_address, sizeof(reuse_address)) < 0) {
        perror("setsockopt");
        close(server_socket);
        database_close(database);
        return 1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);
    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0 ||
        listen(server_socket, 5) < 0) {
        perror("server setup");
        close(server_socket);
        database_close(database);
        return 1;
    }

    printf("Relay Server waiting on port %d...\n", PORT);
    while (1) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        handle_client(client_socket, database, ubuntu_ip);
        close(client_socket);
    }

    close(server_socket);
    database_close(database);
    return 1;
}
