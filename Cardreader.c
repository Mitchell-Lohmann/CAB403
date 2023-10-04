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

// 1023 so can add null term if req
#define BUFFER_SIZE 1023

/* Port Intialisation */
int Port_CardReader = 3001;
int Port_Overseer = 3000;

/* Card reader shared-memory struct initialisation */
typedef struct card_reader_controller
{
    char scanned[16];
    pthread_mutex_t mutex;
    pthread_cond_t scanned_cond;

    char response; // 'Y' or 'N' (or '\0' at first)
    pthread_cond_t response_cond;
} card_reader;

/*Function Definitions*/
void send_looped(int fd, const void *buf, size_t sz);

void send_message(const char *buf);

void *normaloperation_cardreader(void *param);

int connect_to_overseer();

int main(int argc, char **argv)
{
     /* Check for error in input arguments */
    if(argc < 6)
    {
        fprintf(stderr, "Usage: {id} {wait time (in microseconds)} {shared memory path} {shared memory offset} {overseer address:port exit (1)");
        exit(1);
    }

    /* Initialising the card */
    card_reader card;

    strcpy(card.scanned, "0b9adf9c81fb959#");

    /* Initialising the pthread */
    // pthread_t card1;

    /* Initialising the mutex and cond-var */
    pthread_mutex_init(&card.mutex, NULL);
    pthread_cond_init(&card.scanned_cond, NULL);
    pthread_cond_init(&card.response_cond, NULL);

    /* Receive buffer */
    char buf[BUFFER_SIZE];

    strcpy(buf, "CARDREADER {id} HELLO# \n");

    send_message(buf);

    printf("Send this msg to overseer %s \n", buf);

    /* Normal Operations */
    /* Todo : Use pthread to do normal operations for card reader */
    pthread_mutex_lock(&card.mutex);
    for (;;)
    {

        if ((card.scanned[0] != '\0'))
        {
            strcpy(buf, "CARDREADER 101 SCANNED 0b9adf9c81fb959#");
            send_message(buf);
            // Receive msg
            pthread_cond_signal(&card.response_cond);
        }

        pthread_cond_wait(&card.scanned_cond, &card.mutex);
    }

} // end main

int connect_to_overseer()
{
    int fd;

    /*Create TCP IP socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
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

    /* Connect to overseer */
    if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect()");
        exit(1);
    }

    return fd;
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
        ptr += sent;
        remain -= sent;
    }
}

void send_message(const char *buf)
{
    /* Connects to overseer before sending message */
    int fd = connect_to_overseer();

    uint32_t len = htonl(strlen(buf));
    send_looped(fd, &len, sizeof(len));
    send_looped(fd, buf, strlen(buf));

    /* Close connection */
    if (close(fd) == -1)
    {
        perror("close()");
        exit(1);
    }
}
