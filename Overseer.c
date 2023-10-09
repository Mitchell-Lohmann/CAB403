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



#define BUFFER_SIZE 1024


/* Overseer shared memory structure */
typedef struct {
    char security_alarm; // '-' if inactive, 'A' if active
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}shm_overseer;

int Port_CardReader = 3001; 

int Port_Overseer = 3000;



void recv_looped(int fd, void *buf, size_t sz)
{
    char *ptr = buf;
    size_t remain = sz;

    while (remain > 0)
    {
        ssize_t received = read(fd, ptr, remain);
        if (received == -1)
        {
            perror("read()");
            exit(1);
        }
        ptr+= received;
        remain -= received;
    }
}


char *receive_msg(int fd)
{
    uint32_t nlen;
    recv_looped(fd, &nlen, sizeof(nlen));
    uint32_t len = ntohl(nlen);

    char *buf = malloc(len + 1);
    buf[len] = '\0';
    recv_looped(fd, buf, len);
    return buf;
}



int main(int argc, char **argv) 
{
    /* Check for error in input arguments */
    if(argc < 8)
    {
        fprintf(stderr, "Missing command line arguments, {address:port} {door open duration (in microseconds)} {datagram resend delay (in microseconds)} {authorisation file} {connections file} {layout file} {shared memory path} {shared memory offset}");
        exit(1);
    }
    
    /* Initialise input arguments */

    char *full_addr = argv[1];
    char addr[10];
    int port ;
    int door_open_duration = atoi(argv[2]);
    int datagram_resend_delay = atoi(argv[3]);
    char *authorisation_file = argv[4];
    char *connection_file = argv[5];
    char *layout_file = argv[6];
    const char *shm_path = argv[7];
    int shm_offset = atoi(argv[8]);

    
    // Use strtok to split the input string using ':' as the delimiter
    char *token = strtok((char *)full_addr, ":");
    if (token != NULL) {
        // token now contains "127.0.0.1"
          
        memcpy(addr, token, 9);
        addr[9] = '\0';

        token = strtok(NULL, ":");
        if (token != NULL) {
            port = atoi(token); // Store the port as an integer
        } 
        else 
        {
            perror("Invalid input format of port number.\n");
            exit(1);
        }
    } 
    else 
    {
        perror("Invalid input format of address.\n");
        exit(1);

    }

    /* Client file descriptor */
    int clientfd;

    /* receive buffer */
    // char buffer[BUFFER_SIZE];

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
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family =AF_INET;
    servaddr.sin_port = htons(Port_Overseer);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrlen = sizeof(servaddr);

    /* Assign a name to the socket created */
    if (bind(socketfd, (struct sockaddr *)&servaddr, addrlen)==-1) 
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

        char *msg = receive_msg(clientfd);
        
        printf("Received msg from client %s \n", msg);
        free(msg);

         /* Shut down socket - ends communication*/
        if (shutdown(clientfd, SHUT_RDWR) == -1){
            perror("shutdown()");
            exit(1);
        }

		/* close the socket used to receive data */
		if (close(clientfd) == -1)
		{
			perror("exit()");
			exit(1);
		}        
    } // end while
}