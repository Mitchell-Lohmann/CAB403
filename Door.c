#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

int Port_Door = 49153 // Begining of dynamiclly accessible ports

void sendMessage(int fd, const char *msg)
{
    int len = strlen(msg);
    uint32_t netLen = htonl(len);
    send(fd, &netLen, sizeof(netLen), 0);
    if (send(fd, msg, len, 0) != len) 
    {
        fprintf(stderr, "send did not send all data\n");
        exit(1);
    }
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) 
    {
        perror("socket()");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        perror("inet_pton");
    }

addr.sin_family = AF_INET;
addr.sin_port = htons(Port_Door);

// connect
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) 
    {
        perror("connect()");
        return 1;
    }

    char *dataToSend = "123456";
    sendMessage(fd, dataToSend);
    sendMessage(fd, dataToSend);

    if (shutdown(fd, SHUT_RDWR) == -1) 
    {
        perror("shutdown()");
        return 1;
    }

    close(fd);
}
