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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

/* Start Main */

int main(int argc, char **argv)
{
    /* Check for error in input arguments */
    if (argc < 6)
    {
        fprintf(stderr, "Missing command line arguments, {id} {wait time (in microseconds)} {shared memory path} {shared memory offset} {overseer address:port}\n");
        exit(1);
    }

    /* Initialise input arguments */
    int id = atoi(argv[1]);
    const char *shmPath = argv[3];
    int shmOffset = atoi(argv[4]);
    char *fullAddr = argv[5];

    char overseerAddr[10];
    int overseerPort;

    // Use strtok to split the input string using ':' as the delimiter
    char *token = strtok((char *)fullAddr, ":");
    if (token != NULL)
    {
        // token now contains "127.0.0.1"

        memcpy(overseerAddr, token, 9);
        overseerAddr[9] = '\0';

        token = strtok(NULL, ":");
        if (token != NULL)
        {
            overseerPort = atoi(token); // Store the port as an integer
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

    /* Open share memory segment */
    int shm_fd = shm_open(shmPath, O_RDWR, 0666); // Creating for testing purposes

    if (shm_fd == -1)
    {
        perror("shm_open()");
        exit(1);
    }

    /*fstat helps to get information of the shared memory like its size*/
    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1)
    {
        perror("fstat()");
        exit(1);
    }

    char *shm = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    /* shared is used to access the shared memory */
    shm_cardreader *shared = (shm_cardreader *)(shm + shmOffset);

    /* Initialisation */

    /* Send buffer */
    char buff[BUFFER_SIZE];

    /* Write msg into buf */
    sprintf(buff, "CARDREADER %d HELLO#", id);

    /* Send message to overseer */
    tcpSendMessageTo(buff, overseerPort, overseerAddr, 1);

    /* Normal Operation for cardreader */

    /* Lock the mutex */
    pthread_mutex_lock(&shared->mutex);

    for (;;)
    {
        /* Look at the scanned code */
        if (shared->scanned[0] != '\0')
        {
            char buf[17];
            memcpy(buf, shared->scanned, 16);
            buf[16] = '\0';

            sprintf(buff, "CARDREADER %d SCANNED %s#", id, buf);

            /* Connects to overseer and sends the message in the buffer*/
            unsigned int overseerFd = tcpSendMessageTo(buff, overseerPort, overseerAddr, 0);
            if (overseerFd == -1)
            {
                fprintf(stderr, "send_message_to");
                exit(1);
            }

            /* Receive message from overseer */
            receiveMessage(overseerFd, buff, sizeof(buff));

            if (strcmp(buff, "ALLOWED#") == 0)
            {
                shared->response = 'Y';
            }
            else if (strcmp(buff, "DENIED#") == 0)
            {
                shared->response = 'N';
            }
            else
            {
                printf("Cardreader received %s from overseer\n", buff);
                shared->response = 'N';
            }
            pthread_cond_signal(&shared->response_cond);
        }
        pthread_cond_wait(&shared->scanned_cond, &shared->mutex);
    }
    close(shm_fd);

} /* end main */
