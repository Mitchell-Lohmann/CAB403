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
#include <fcntl.h>
#include "common.h"

/* Code is written to be complaint with saftey standards  MISRA-C and IEC 61508. */

/* The 'min detections' value will be <= 50 (which means the detections list only needs to be 50 entries large) */
struct timeval detections[50];

/* There will be <= 100 fail-safe doors, which means the fail-safe doors list only needs to be 100 entries large */
struct door_reg_datagram fail_safe_doors[100];

/* Function declaration */
void removeOldTimeStamps(struct timeval *detections, int *numDetections, int detection_period);
int isTimeStampOld(struct datagram_format *datagram, int detection_period);
int isNewDoor(struct door_reg_datagram *new_door, struct door_reg_datagram fail_safe_doors[], int *num_doors);
int setFireAlarm(shm_firealarm *shm, struct door_reg_datagram *doors, int numDoors);
struct door_confirm_datagram initialise_DREG_Struct(struct door_reg_datagram *door);

int main(int argc, char **argv)
{
    /* Check for error in input arguments */
    if (argc < 9)
    {
        fprintf(stderr, "Missing command line arguments, {address:port} {temperature threshold} {min detections} {detection period (in microseconds)} {reserved argument} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    /* Initialise input arguments */
    char *fireaklarmFullAddr = argv[1];
    char firealarmAddr[10];
    int firealarmPort = splitAddressPort(fireaklarmFullAddr, firealarmAddr);
    int tempThreshold = atoi(argv[2]);
    int minDetections = atoi(argv[3]);
    int detectionPeriod = atoi(argv[4]);
    const char *shmPath = argv[6];
    int shmOffset = atoi(argv[7]);
    char overseerAddr[10];
    int overseerPort = splitAddressPort(argv[8], overseerAddr);
    /* Safe buffer size for UDP communication */
    char buff[512]; 
    int numDetections = 0;
    int numDoors = 0;
    /* variable to track if fire alarm is set */
    int ifFireAlarmSet = 0; 
    /* Initialize the UDP socket for receiving messages */
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

    /* Initialise the address struct for fire alarm */
    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(firealarmPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the UDP socket*/
    if (bind(recvsockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind()");
        exit(1);
    }

    /* Create send socket to send UDP datagram to overseer */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address of overseer (address + Port) */
    struct sockaddr_in sendtoaddr;
    (void)memset(&sendtoaddr, 0, sizeof(sendtoaddr));
    sendtoaddr.sin_family = AF_INET;
    sendtoaddr.sin_port = htons(overseerPort);
    if (inet_pton(AF_INET, overseerAddr, &sendtoaddr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", overseerAddr);
        exit(1);
    }
    socklen_t senderaddr_len = sizeof(sendtoaddr);

    /* Write msg into buf */
    sprintf(buff, "FIREALARM %s:%d HELLO#\n", firealarmAddr, firealarmPort);

    /* Send initialisation message to overseer */
    if (tcpSendMessageTo(buff, overseerPort, overseerAddr, 1) == -1)
    {
        perror("send_message_to()");
        exit(1);
    }

    /* Open share memory segment */
    int shm_fd = shm_open(shmPath, O_RDWR, 0666);s

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
        exit(1);
    }

    char *shm = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    /* shared is used to access the shared memory */
    shm_firealarm *shared = (shm_firealarm *)(shm + shmOffset);

    /* Normal function */

    for (;;)
    {
        /* Receives first 4 bytes and stores into buffer */
        ssize_t bytes = recvfrom(recvsockfd, buff, 512, 0, NULL, NULL);
        if (bytes == -1)
        {
            perror("recvfrom()");
            exit(1);
        }

        if ((strncmp(buff, "FIRE", 4) == 0) && !ifFireAlarmSet)
        {
            /* fire emergency datagram */
            ifFireAlarmSet = setFireAlarm(shared, fail_safe_doors, numDoors);
        }
        else if ((strncmp(buff, "TEMP", 4) == 0) && !ifFireAlarmSet)
        {
            /* Temp update datagram */
            struct datagram_format *pointer = (struct datagram_format *)buff;
            if (pointer == NULL)
            {
                printf("Pointer points to NULL");
                exit(1);
            }
            if (pointer->temperature < tempThreshold)
            {
                /* Loop back to start */
                continue;
            }
            else if (isTimeStampOld(pointer, detectionPeriod))
            {
                /* Loop back to start */
                continue;
            }
            /* Delete all timestamps older than {detection period} microseconds from the detection list */
            removeOldTimeStamps(detections, &numDetections, detectionPeriod);
            /* Add timestep to detections array */
            if (numDetections < 50)
            {
                /* Add timestamp to detections array */
                detections[numDetections] = pointer->timestamp;
                numDetections++;
            }
            else
            {
                perror("Array is full. Cannot add more elements. \n");
                exit(1);
            }

            /* Check if there is at least {min detections} entries in the detection list */
            if (numDetections >= minDetections)
            {

                ifFireAlarmSet = setFireAlarm(shared, fail_safe_doors, numDoors);
            }
            else
            {
                /* Loop to start */
                continue;
            }
        }
        else if ((strncmp(buff, "DOOR", 4) == 0) && !ifFireAlarmSet)
        {
            /* Door registration datagram */
            struct door_reg_datagram *pointer = (struct door_reg_datagram *)buff;
            if (pointer == NULL)
            {
                printf("Pointer points to NULL");
                exit(1);
            }

            /* If door not in the list add it */
            if (isNewDoor(pointer, fail_safe_doors, &numDoors))
            {
                /* checks if max number of allowable doors reached */
                if (numDoors < 100)
                {
                    /* Dereference pointer to access the data */
                    fail_safe_doors[numDoors] = *pointer;
                    numDoors++;
                }
                else
                {
                    perror("Array is full. Cannot add more elements. \n");
                    exit(1);
                }
            }
            else
            {
                /* Loop to start */
                continue;
            }

            /* Create an instance of DREG datagram */
            struct door_confirm_datagram confirm_door = initialise_DREG_Struct(pointer);

            /* Send door confirmation datagram to overseer */
            (void)sendto(sendfd, &confirm_door, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, senderaddr_len);

            /* Loop to start */
            continue;
        }
        else if ((strncmp(buff, "DOOR", 4) == 0) && ifFireAlarmSet)
        {
            /* Door registration datagram */
            struct door_reg_datagram *pointer = (struct door_reg_datagram *)buff;
            if (pointer == NULL)
            {
                printf("Pointer points to NULL");
                exit(1);
            }

            char *buf = "OPEN_EMERG#";

            /* Convert the in_addr to a string */
            char *addr_str = inet_ntoa(pointer->door_addr);

            /* Send OPEN_EMERG# to newly registered door */
            tcpSendMessageTo(buf, pointer->door_port, addr_str, 1);

            /* Create an instance of DREG datagram */
            struct door_confirm_datagram confirm_door = initialise_DREG_Struct(pointer);

            /* Send door confirmation datagram to overseer */
            (void)sendto(sendfd, &confirm_door, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, senderaddr_len);

            /* Loop to start */
            continue;
        }
    }

} /* End main */

/* Function Definitions */

//<summary>
// Function to remove old timestamps
//</summary>
void removeOldTimeStamps(struct timeval *detections, int *numDetections, int detection_period)
{
    struct timeval current_time;
    /* Get the current time */
    gettimeofday(&current_time, NULL);

    int numValidDetections = 0;

    for (int i = 0; i < *numDetections; i++)
    {

        int time_elapsed = (current_time.tv_sec - detections[i].tv_sec) * 1000000 +
                           (current_time.tv_usec - detections[i].tv_usec);

        /* Check if the time difference is less than the detection period */
        if (time_elapsed < detection_period)
        {
            detections[numValidDetections] = detections[i];
            numValidDetections++;
        }
    }
    
    /* Update the number of valid detections */
    *numDetections = numValidDetections; 
}

//<summary>
// Function to check the timestamp
//</summary>
int isTimeStampOld(struct datagram_format *datagram, int detection_period)
{
    struct timeval current_time;
    /* Get the current time */
    gettimeofday(&current_time, NULL);

    /* Calculate the time difference in microseconds */
    int time_elapsed = (current_time.tv_sec - datagram->timestamp.tv_sec) * 1000000 +
                       (current_time.tv_usec - datagram->timestamp.tv_usec);

    return time_elapsed > detection_period;
}

//<summary>
// Function to check if the Door is present in the door list
//</summary>
int isNewDoor(struct door_reg_datagram *new_door, struct door_reg_datagram fail_safe_doors[], int *num_doors)
{
    /* Check if the new door is already in the list */
    for (int i = 0; i < *num_doors; i++)
    {
        if (htonl(fail_safe_doors[i].door_port) == htonl(new_door->door_port))
        {
            /* The door is already in the list, no need to add it again */
            return 0;
        }
    }
    return 1;
}



//<summary>
// Function that sets fire alarm in shared memory and sends OPEN_EMERG# to every fail_safe doors.
//</summary>
int setFireAlarm(shm_firealarm *shm, struct door_reg_datagram doors[], int numDoors)
{
    char *buff = "OPEN_EMERG#";

    pthread_mutex_lock(&shm->mutex);
    shm->alarm = 'A';
    pthread_mutex_unlock(&shm->mutex);
    pthread_cond_signal(&shm->cond);

    for (int i = 0; i < numDoors; i++)
    {
        /* Convert in_port_t to int */
        int doorPort = htons(doors[i].door_port);
        /* Convert the in_addr to a string */
        char *addr_str = inet_ntoa(doors[i].door_addr);
        if (tcpSendMessageTo(buff, doorPort, addr_str, 1) == -1)
        {
            perror("send_message_to()");
            exit(1);
        }
    }
    return 1;
}

//<summary>
// 
//</summary>
struct door_confirm_datagram initialise_DREG_Struct(struct door_reg_datagram *door)
{
    struct door_confirm_datagram new_door;
    /* Write DREG to header */
    (void)memcpy(new_door.header, "DREG", sizeof(new_door.header));
    /* Write addr and port of door to DREG datagram */
    new_door.door_addr = door->door_addr;
    new_door.door_port = door->door_port;

    return new_door;
}
