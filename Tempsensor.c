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
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include "common.h"

int main(int argc, char **argv)
{

    /* Check for error in input arguments */
    if (argc < 8)
    {
        fprintf(stderr, "Missing command line arguments, {id} {address:port} {max condvar wait (microseconds)} {max update wait (microseconds)} {shared memory path} {shared memory offset} {receiver address:port}...");
        exit(1);
    }

    int id = atoi(argv[1]);
    char *full_addr = argv[2];
    char tempsensorAddr[10];
    int tempsensorPort = splitAddressPort(full_addr, tempsensorAddr);
    int maxCondvarWait = atoi(argv[3]);
    int maxUpdateWait = atoi(argv[4]);
    char *shmPath = argv[5];
    int shmOffset = atoi(argv[6]);
    int receiverNum = argc - 7; // Total number of receiving tempsensors
    char receiverAddr[10];      // All receivers have same addr

    int receiverPort[receiverNum]; // Array storing receiver_port

    for (int i = 0; i < receiverNum; i++)
    {
        char *receiverFullAddr = argv[7 + i];
        receiverPort[i] = splitAddressPort(receiverFullAddr, receiverAddr);
    }

    /* Open share memory segment */
    int shmFD = shm_open(shmPath, O_RDWR, 0666); // Creating for testing purposes

    if (shmFD == -1)
    {
        perror("shm_open()");
        exit(1);
    }

    /* fstat helps to get information of the shared memory like its size */
    struct stat shm_stat;
    if (fstat(shmFD, &shm_stat) == -1)
    {
        perror("fstat()");
        exit(1);
    }

    char *shm = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFD, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    /* shared is used to access the shared memory */
    shm_tempsensor *shared = (shm_tempsensor *)(shm + shmOffset);

    /* Normal operation */

    /* Create an addr_entry for this tempsensor */
    struct addr_entry sensorAddr;
    sensorAddr.sensor_port = htons(tempsensorPort);
    if (inet_pton(AF_INET, tempsensorAddr, &sensorAddr.sensor_addr) <= 0)
    {
        perror("inet_pton()");
        exit(1);
    }

    /* UDP initialisation */

    /* Create a socket for receiving the datagram */
    int recvsockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvsockfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Enable for re-use of address */
    int opt_enable = 1;
    if (setsockopt(recvsockfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1)
    {
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct for this temp sensor */
    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tempsensorPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the UDP socket*/
    if (bind(recvsockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind()");
        exit(1);
    }

    /* Create a socket for sending the datagram */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Declare sockaddr_in struct for destination address and client addr*/
    struct sockaddr_in destaddr, clientaddr;
    socklen_t client_size = sizeof(clientaddr);

    struct timeval last_update_time;
    struct timeval current_time;
    struct timespec max_wait_time;

    int loopCount = 1;

    float tempReading;
    pthread_mutex_lock(&shared->mutex);

    for (;;)
    {
        /* Read temp */
        tempReading = shared->temperature;
        pthread_mutex_unlock(&shared->mutex);

        /* Calculate the time elapsed since the last update */
        /* NULL indicates that timezone info is not needed */
        gettimeofday(&current_time, NULL);
        int elapsed_time = (current_time.tv_sec - last_update_time.tv_sec) * 1000000 + (current_time.tv_usec - last_update_time.tv_usec);

        /* If the temperature has changed OR if this is the first iteration of this loop OR if the max update
        wait has passed since this sensor last sent an update */
        if (loopCount == 1 || tempReading != shared->temperature || elapsed_time >= maxUpdateWait)
        {
            /* Construct UDP datagram */
            struct datagram_format datagram;
            memcpy(datagram.header, "TEMP", 4);
            datagram.id = id;
            datagram.temperature = tempReading;

            gettimeofday(&current_time, NULL);
            datagram.timestamp = current_time;

            /* Add addr of this tempsensor */
            datagram.address_count = 1;
            datagram.address_list[0] = sensorAddr;

            /* Send datagram to each receiver */
            for (int i = 0; i < receiverNum; i++)
            {
                socklen_t destaddr_len = sizeof(destaddr);
                (void)memset(&destaddr, 0, sizeof(destaddr));
                destaddr.sin_family = AF_INET;
                destaddr.sin_port = htons(receiverPort[i]);
                if (inet_pton(AF_INET, receiverAddr, &destaddr.sin_addr) != 1)
                {
                    fprintf(stderr, "inet_pton(%s)\n", receiverAddr);
                    exit(1);
                }

                int bytes_sent = sendto(sendfd, &datagram, sizeof(datagram), 0, (struct sockaddr *)&destaddr, destaddr_len);
                if (bytes_sent == -1)
                {
                    perror("sendto()");
                    exit(1);
                }
            }
            /* Update the last update time */
            gettimeofday(&last_update_time, NULL);
        }

        loopCount++;

        /* Receive next datagram if any */
        struct datagram_format receivedDatagram;
        int bytes_received = recvfrom(recvsockfd, &receivedDatagram, sizeof(receivedDatagram), MSG_DONTWAIT, (struct sockaddr *)&clientaddr, &client_size);
        if (bytes_received == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* No data is currently available */
                continue;
            }
            else
            {
                perror("recvfrom()");
                exit(1);
            }
        }
        else
        {
            /* Datagram received successfully */
            /* If list if full remove the oldest address */
            if (receivedDatagram.address_count == 50)
            {
                for (int i = 0; i < 50; i++)
                {
                    receivedDatagram.address_list[i] = receivedDatagram.address_list[i + 1];
                }
                receivedDatagram.address_count--;
            }

            /* Update received datagrams address list with the current temp sensors sensor addr */
            receivedDatagram.address_list[receivedDatagram.address_count] = sensorAddr;
            receivedDatagram.address_count++;

            /* Send this new datagram to each receiver if receiver not in address list */
            for (int i = 0; i < receiverNum; i++)
            {
                /* Check if receiver present in the address list of the received datagram */
                bool match = false;
                /* Loop to check if the receiver in the datagrams address list */
                for (int j = 0; j < receivedDatagram.address_count; j++)
                {
                    /* Check if ther is a match */
                    if (match == false)
                    {
                        in_port_t port_entry = receivedDatagram.address_list[j].sensor_port;

                        /* Compare to receiver port */
                        if (htons(receiverPort[i]) == port_entry)
                        {
                            match = true;
                            break;
                        }
                    }
                }

                if (!match)
                {
                    /* Send the updated datagram to the current receiver if it is not in the received datagrams address list */
                    socklen_t destaddr_len = sizeof(destaddr);
                    (void)memset(&destaddr, 0, sizeof(destaddr));
                    destaddr.sin_family = AF_INET;
                    destaddr.sin_port = htons((in_port_t)receiverPort[i]);
                    if (inet_pton(AF_INET, receiverAddr, &destaddr.sin_addr) != 1)
                    {
                        fprintf(stderr, "inet_pton(%s)\n", receiverAddr);
                        exit(1);
                    }

                    int bytes_sent = sendto(sendfd, &receivedDatagram, sizeof(receivedDatagram), 0, (struct sockaddr *)&destaddr, destaddr_len);
                    if (bytes_sent == -1)
                    {
                        perror("sendto()");
                        exit(1);
                    }
                }
            }
        }

        /* Update timespec max_wait_time to represent the value of max_condvar_wait */
        gettimeofday(&current_time, NULL);
        max_wait_time.tv_sec = current_time.tv_sec;
        max_wait_time.tv_nsec = (current_time.tv_usec + maxCondvarWait) * 1000;

        /* Lock the mutex */
        pthread_mutex_lock(&shared->mutex);
        int result = pthread_cond_timedwait(&shared->cond, &shared->mutex, &max_wait_time);
        if (result == -1)
        {
            if (errno == ETIMEDOUT)
            {
                // Timeout occured
                perror("Cond wait timed out");
            }
            else
            {
                // Other error occurred. Handle it as needed.
                perror("pthread_cond_timedwait()");
                exit(1);
            }
        }
    }
    return 0;
}