#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>



//1023 so can add null term if req
#define BUFFER_SIZE 1023
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

int Port_Overseer = 3000;

int main(int argc, char **argv) 
{
    /* Client file descriptor */
    int clientfd;

    /* receive buffer */
    char buffer[BUFFER_SIZE];

    /*Create TCP IP Socket*/
    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* enable for re-use address*/
    int opt_enable = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1){
        perror("setsockopt()");
        exit(1);
    }

    /*Declare a data structure to specify the socket address (IP address + Port)
    *memset is used to zero the struct out
    */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family =AF_INET;
    addr.sin_port = htons(Port_Overseer);
    addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrlen = sizeof(addr);

    /* Assign a name to the socket created */
    if (bind(socketfd, (struct sockaddr *)&addr, addrlen)==-1) 
    {
        perror("bind()");
        exit(1);
    }

    /*Place server in passive mode - listen for incoming cient request*/
    if (listen(socketfd, 100)==-1) 
    {
        perror("listen()");
        exit(1);
    }

    /* Infinite Loop */
    while (1) 
    {
        /* Generate a new socket for data transfer with the client */

        /* The argument addr is a pointer to a sockaddr structure.  This structure
        is filled in with the address of the peer socket, as known to the  com‐
        munications  layer.   The  exact format of the address returned addr is
        determined by the socket's address family (see socket(2)  and  the  re‐
        spective protocol man pages).  When addr is NULL, nothing is filled in;
        in this case, addrlen is not used, and should also be NULL.*/

        /* Left Null for now might have to change */
        clientfd = accept(socketfd, NULL, NULL );
        if (clientfd==-1) 
        {
            perror("accept()");
            exit(1);
        }

        
        size_t bytesRcv = recv(clientfd, buffer, BUFFER_SIZE, 0);
        if (bytesRcv==-1) 
        {
            perror("bytesrcv");
            exit(1);

        }

		/* add null terminator to received data and print out message */
        buffer[bytesRcv] ='\0';
        printf("%s\n", buffer);

		/* close the socket used to receive data */
		if (close(clientfd) == -1)
		{
			perror("exit()");
			exit(1);
		}        
    } // end while

    /* Currently not shutting down the server */
    /* shutdown the connection - end communication to and from the socket SHUT_RDWR */
}