#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "10.10.16.51"
#define PORT 5001
#define BUF_SIZE 256

int main(void)
{
    int sock;
    struct sockaddr_in serv_addr;
    char buf[BUF_SIZE] = {0};

    sock = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    write(sock, "Relay Pi Test", strlen("Relay Pi Test"));

    read(sock, buf, sizeof(buf) - 1);
    printf("ACK: %s\n", buf);

    close(sock);

    return 0;
}

