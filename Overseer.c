#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int Port_Door = 49153; // Begining of dynamiclly accessible ports

int main(int argc, char **argv) 
{
    if (argc != 2) {
        printf("\nUsage a.out <portNo>\n");
        return 1;
    }

    int bytesRcv;
    struct sockaddr clientaddr;
    socklen_t addrlen;
    char buffer[1024];
    struct sockaddr_in serverAddress;
    int fd = socket(AF_INET, SOCK_STREAM, 0); //0 is default for socket implementation

    if (fd==-1) {
        perror("\nsocket()\n");
        return 1;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
    serverAddress.sin_port = htons(Port_Door);

    if (bind(fd, (struct sockaddr *)&serverAddress, sizeof(serverAddress))==-1) 
    {
        perror("bind()");
        return 1;
    }
    int clientfd;
    if (listen(fd, 10)==-1) 
    {
        perror("listen()");
        return 1;
    }

    while (1) 
    {
        clientfd = accept(fd,&clientaddr, &addrlen );
        if (clientfd==-1) 
        {
            perror("accept()");
            return 1;
        }

        //1023 so can add null term if req
        bytesRcv = recv(clientfd, buffer, 1023,0);
        if (bytesRcv==-1) 
        {
            perror("bytesrcv");
            return 1;
        }

        buffer[bytesRcv] ='\0';
        printf("\nNumber of Bytes received from client was %d.\n\nInformation sent through socket --> %s\n\n", bytesRcv, buffer);
        close(clientfd);
    }

    if (shutdown(clientfd, SHUT_RDWR) ==-1) 
    {
        perror("shutdown()");
        return 1;
    }

    close(fd); //sockets can remain open after program termination - when open socket should close it
}