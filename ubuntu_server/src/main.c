#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "database.h"
#include "json.h"
#include "server.h"

#define PORT 5001

int main(void)
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr;
    char buf[MAX_MESSAGE_SIZE + 1];
    char ack[MAX_MESSAGE_SIZE + 1];
    VisionMessage message;
    MYSQL *database;
    int save_result;

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

    clnt_sock = accept(serv_sock, NULL, NULL);
    if (clnt_sock < 0) {
        perror("accept");
        close(serv_sock);
        database_close(database);
        return 1;
    }

    if (recv_frame(clnt_sock, buf, sizeof(buf)) <= 0) {
        fprintf(stderr, "Failed to receive frame\n");
        close(clnt_sock);
        close(serv_sock);
        database_close(database);
        return 1;
    }

    printf("Received: %s\n", buf);

    if (parse_vision_json(buf, &message) < 0) {
        fprintf(stderr, "Invalid JSON message\n");
        close(clnt_sock);
        close(serv_sock);
        database_close(database);
        return 1;
    }

    save_result = database_save_vision(database, &message);
    if (create_ack_json(message.message_id,
                        save_result == DB_SAVE_ERROR ? "error" : "ok",
                        save_result == DB_SAVE_DUPLICATE,
                        save_result == DB_SAVE_ERROR ? "DATABASE_ERROR" : NULL,
                        ack, sizeof(ack)) < 0) {
        fprintf(stderr, "Failed to create ACK JSON\n");
        close(clnt_sock);
        close(serv_sock);
        database_close(database);
        return 1;
    }

    if (send_frame(clnt_sock, ack) < 0) {
        fprintf(stderr, "Failed to send ACK frame\n");
    }

    close(clnt_sock);
    close(serv_sock);
    database_close(database);

    return save_result == DB_SAVE_ERROR ? 1 : 0;
}
