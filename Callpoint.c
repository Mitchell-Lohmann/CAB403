#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include "common.h"

/* Code is written to be complaint with saftey standards  MISRA-C and IEC 61508. */

int main(int argc, char *argv[])
{
    /* Check for error in input arguments */
    if (argc < 5)
    {
        fprintf(stderr, "Missing command line arguments, {resend delay (in microseconds)} {shared memory path} {shared memory offset} {fire alarm unit address:port}\n");
        exit(1);
    }

    /* Initialise input arguments */
    int resendDelay = atoi(argv[1]);
    const char *shmPath = argv[2];
    int shm_offset = atoi(argv[3]);
    char firealarmAddr[10];
    int firealarmPort = splitAddressPort(argv[4], firealarmAddr);

    /* Open shared memory segment */
    int shm_fd = shm_open(shmPath, O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open()");
        exit(1);
    }

    /* fstat helps to get information of the shared memory like its size */
    struct stat shm_stat;
    if (fstat(shm_fd, &shm_stat) == -1)
    {
        perror("fstat()");
        close(shm_fd);
        exit(1);
    }

    char *shm = mmap(NULL, (size_t)shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap()");
        close(shm_fd);
        exit(1);
    }

    close(shm_fd);

    /* shared is used to access the shared memory */
    shm_callpoint *shared = (shm_callpoint *)(shm + shm_offset);

    /* UDP */
    /* Sets the datagram to be send */
    callpoint_datagram datagram;
    (void)memcpy(datagram.header, "FIRE", sizeof(datagram.header));

    /* Create a socket for sending the datagram */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address (Address + Port)
    memset is used to zero the struct out */
    struct sockaddr_in destaddr;
    socklen_t destaddr_len = sizeof(destaddr);
    (void)memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_port = htons((in_port_t)firealarmPort);
    if (inet_pton(AF_INET, firealarmAddr, &destaddr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", firealarmAddr);
        exit(1);
    }

    /* Locks the mutex */
    (void)pthread_mutex_lock(&shared->mutex);
    for (;;)
    {
        if (shared->status == '*')
        {
            /* Sends UDP Datagram */
            (void)sendto(sendfd, datagram.header, (size_t)strlen(datagram.header), 0, (const struct sockaddr *)&destaddr, destaddr_len);
            /* Sleep for {resend delay} */
            (void)usleep(resendDelay);
        }
        else
        {
            (void)pthread_cond_wait(&shared->cond, &shared->mutex);
        }
    }
} /* End main */