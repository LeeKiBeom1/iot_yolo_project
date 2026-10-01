#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "database.h"
#include "json.h"
#include "server.h"
#include "sync.h"
#include "vision_stream.h"
#include "vision_io.h"

#define PORT 5000
static int upstream_port = 5001;
#define UBUNTU_PORT upstream_port
#define DATABASE_PATH "db/road_monitor.db"

typedef struct {
    int client_socket;
    sqlite3 *database;
    const char *ubuntu_ip;
} ClientContext;

static pthread_mutex_t database_mutex = PTHREAD_MUTEX_INITIALIZER;

static void handle_client(int client_socket, sqlite3 *database,
                          const char *ubuntu_ip)
{
    char buffer[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];
    char type[16];
    const char *message_id;
    SensorMessage sensor_message;
    VisionMessage vision_message;
    TrafficCountMessage traffic_count_message;
    int receive_result;
    int save_result;
    int sync_result;

    while (1) {
        receive_result = recv_frame(client_socket, buffer, sizeof(buffer));
        if (receive_result == 0) break;
        if (receive_result < 0) {
            fprintf(stderr, "Failed to receive frame\n");
            break;
        }

        if (parse_message_type(buffer, type, sizeof(type)) < 0) {
            fprintf(stderr, "Invalid JSON message\n");
            break;
        }

        if (strcmp(type, "sensor") == 0) {
            if (parse_sensor_json(buffer, &sensor_message) < 0) {
                fprintf(stderr, "Invalid sensor message\n");
                break;
            }
            message_id = sensor_message.message_id;
            pthread_mutex_lock(&database_mutex);
            save_result = database_save_sensor(database, &sensor_message);
            pthread_mutex_unlock(&database_mutex);
        } else if (strcmp(type, "vision") == 0) {
            if (parse_vision_json(buffer, &vision_message) < 0) {
                fprintf(stderr, "Invalid vision message\n");
                break;
            }
            message_id = vision_message.message_id;
            pthread_mutex_lock(&database_mutex);
            save_result = database_save_vision(database, &vision_message);
            pthread_mutex_unlock(&database_mutex);
        } else if (strcmp(type, "traffic_count") == 0) {
            if (parse_traffic_count_json(buffer, &traffic_count_message) < 0) {
                fprintf(stderr, "Invalid traffic count message\n");
                break;
            }
            message_id = traffic_count_message.message_id;
            pthread_mutex_lock(&database_mutex);
            save_result = database_save_traffic_count(
                database, &traffic_count_message);
            pthread_mutex_unlock(&database_mutex);
        } else {
            fprintf(stderr, "Unsupported message type\n");
            break;
        }

        /* Arduino is send-only; failure to deliver an unsolicited ACK must
           not prevent its already-persisted sample from being synchronized. */
        if (strcmp(type, "sensor") != 0 && (create_ack_json(message_id,
                            save_result == DB_SAVE_OK ||
                            save_result == DB_SAVE_DUPLICATE ? "ok" : "error",
                            save_result == DB_SAVE_DUPLICATE,
                            save_result == DB_SAVE_CONFLICT
                                ? "MESSAGE_ID_CONFLICT"
                                : save_result == DB_SAVE_ERROR
                                    ? "DATABASE_ERROR" : NULL,
                            ack, sizeof(ack)) < 0 ||
            send_frame(client_socket, ack) < 0)) {
            break;
        }

        if ((save_result == DB_SAVE_OK ||
             save_result == DB_SAVE_DUPLICATE)) {
            pthread_mutex_lock(&database_mutex);
            if (strcmp(type, "sensor") == 0) {
                sync_result = sync_unsent_sensors(
                    database, ubuntu_ip, UBUNTU_PORT);
            } else if (strcmp(type, "vision") == 0) {
                sync_result = sync_unsent_vision(
                    database, ubuntu_ip, UBUNTU_PORT);
            } else {
                sync_result = sync_unsent_traffic_counts(
                    database, ubuntu_ip, UBUNTU_PORT);
            }
            pthread_mutex_unlock(&database_mutex);
        } else {
            sync_result = 0;
        }
        if (sync_result < 0) {
            fprintf(stderr, "Ubuntu sync deferred\n");
        }
    }
}

static void *client_thread(void *argument)
{
    ClientContext *context = argument;

    handle_client(context->client_socket, context->database,
                  context->ubuntu_ip);
    close(context->client_socket);
    free(context);
    return NULL;
}

int main(void)
{
    int port = vision_port("SENSOR_PORT", PORT);
    int server_socket;
    int client_socket;
    int reuse_address = 1;
    pthread_t thread;
    ClientContext *context;
    struct sockaddr_in server_address;
    sqlite3 *database;
    const char *ubuntu_ip = getenv("UBUNTU_SERVER_IP");

    upstream_port = vision_port("SENSOR_FINAL_PORT", 5001);
    if (port < 0 || upstream_port < 0) return 1;

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
    server_address.sin_port = htons((uint16_t)port);
    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0 ||
        listen(server_socket, 5) < 0) {
        perror("server setup");
        close(server_socket);
        database_close(database);
        return 1;
    }

    printf("Relay Server waiting on port %d...\n", port);
    if (pthread_create(&thread, NULL, vision_service, NULL) != 0) return 1;
    pthread_detach(thread);
    if (sync_unsent_sensors(database, ubuntu_ip, UBUNTU_PORT) < 0) {
        fprintf(stderr, "Startup sync deferred\n");
    }
    /* Historical Vision UNSENT rows remain for manual disposition; the new
       real-time channel must not replay them automatically at startup. */
    if (sync_unsent_traffic_counts(database, ubuntu_ip, UBUNTU_PORT) < 0) {
        fprintf(stderr, "Startup traffic count sync deferred\n");
    }

    while (1) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        context = malloc(sizeof(*context));
        if (context == NULL) {
            close(client_socket);
            continue;
        }
        context->client_socket = client_socket;
        context->database = database;
        context->ubuntu_ip = ubuntu_ip;

        if (pthread_create(&thread, NULL, client_thread, context) != 0) {
            close(client_socket);
            free(context);
            continue;
        }
        pthread_detach(thread);
    }

    close(server_socket);
    database_close(database);
    return 1;
}
