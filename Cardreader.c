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
#include <pthread.h>

// Test merging with version 0.1

//1023 so can add null term if req
#define BUFFER_SIZE 1023

/* Port Intialisation */
int Port_CardReader = 3001; 
int Port_Overseer = 3000;

/* Card reader struct initialisation */
typedef struct card_reader_controller{
    char scanned[16];
    pthread_mutex_t mutex;
    pthread_cond_t scanned_cond;
    
    char response ; // 'Y' or 'N' (or '\0' at first)
    pthread_cond_t response_cond;
}card_reader;


/*Function Definitions*/
void send_looped(int fd, const void *buf, size_t sz);

void send_message(int fd, const char *buf);


int main()
{
    /* Initialising the card */
    // card_reader card;
    // card.response ='\0';
    

    /* Receive buffer */
    char buf[BUFFER_SIZE];
    
    /*Create TCP IP socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /*Declare a data structure to specify the socket address (IP address + Port)
    *memset is used to zero the struct out
    */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Port_Overseer);
    const char *ipaddress = "127.0.0.1";
    if (inet_pton(AF_INET, ipaddress, &addr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", ipaddress);
        exit(1);
    }

    /*man (2) connect*/
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect()");
        exit(1);
    }

    strcpy(buf,"CARDREADER {id} HELLO# \n");

    send_message(sockfd, buf);

    printf("Send this msg to overseer %s \n", buf);


    
    /* Shut down socket - ends communication*/
    if (shutdown(sockfd, SHUT_RDWR) == -1)
    {
        perror("shutdown()");
        exit(1);
    }

    if (close(sockfd) == -1)
    {
        perror("close()");
        exit(1);
    }
}

void send_looped(int fd, const void *buf, size_t sz)
{
    const char *ptr = buf;
    size_t remain = sz;
    while (remain > 0)
    {
        ssize_t sent = write(fd, ptr, remain);
        if (sent == -1)
        {
            perror("write()");
            exit(1);
        }
        ptr+= sent;
        remain -= sent;
    }
}

void send_message(int fd, const char *buf)
{
    uint32_t len = htonl(strlen(buf));
    send_looped(fd, &len, sizeof(len));
    send_looped(fd, buf, strlen(buf));

}