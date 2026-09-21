#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 5001
#define BUF_SIZE 256

int main(void)
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr;
    char buf[BUF_SIZE] = {0};

    serv_sock = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(serv_sock, 5);

    printf("Ubuntu Server waiting on port %d...\n", PORT);

    clnt_sock = accept(serv_sock, NULL, NULL);

    read(clnt_sock, buf, sizeof(buf) - 1);
    printf("Received: %s\n", buf);

    write(clnt_sock, "OK", 2);

    close(clnt_sock);
    close(serv_sock);

    return 0;
}

