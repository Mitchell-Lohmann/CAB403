#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>


// <summary>


// </summary>
//<param>      </param>
//<return>     </return>
typedef struct {
    char security_alarm; // '-' if inactive, 'A' if active
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}overseer_struct;





int Port_CardReader = 3001; 

int main(int argc, char **argv) 
{
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
    serverAddress.sin_port = htons(Port_CardReader);

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