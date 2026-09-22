#ifndef SERVER_H
#define SERVER_H

#define MAX_MESSAGE_SIZE 4096

int send_frame(int socket_fd, const char *json);
int recv_frame(int socket_fd, char *buffer, int buffer_size);

#endif
