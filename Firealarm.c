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

/* Call point unit shared memory struct initialisation */
typedef struct
{
    char alarm; /* '-' if inactive, 'A' if active */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_firealarm;

struct addr_entry {
  struct in_addr sensor_addr;
  in_port_t sensor_port;
};

struct temp_datagram_format {
  char header[4]; // {'T', 'E', 'M', 'P'}
  struct timeval timestamp;
  float temperature;
  uint16_t id;
  uint8_t address_count;
  struct addr_entry address_list[50];
};

struct door_reg_datagram{
    char header[4]; // {'D', 'O', 'O', 'R'}
    struct in_addr door_addr;
    in_port_t door_port;
};



// The 'min detections' value will be <= 50 (which means the detections list only needs to be 50 entries large)
struct timeval detections[50];

// There will be <= 100 fail-safe doors, which means the fail-safe doors list only needs to be 100 entries large
struct door_reg_datagram fail_safe_doors[100];

/* Function Definitions */

// Function to remove old timestamps
void removeOldTimestamps(struct timeval* detections, int* numDetections, int detection_period) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL); // Get the current time

    int numValidDetections = 0;
    
    for (int i = 0; i < *numDetections; i++) {

        int time_diff = (current_time.tv_sec - detections[i].tv_sec) * 1000000 +
                    (current_time.tv_usec - detections[i].tv_usec);
        
        // Check if the time difference is less than the detection period
        if (time_diff > detection_period) {
            detections[numValidDetections] = detections[i];
            numValidDetections++;
        }
    }

    *numDetections = numValidDetections; // Update the number of valid detections
}

// Function to check the timestamp
int isTimestampOld(struct temp_datagram_format *datagram, int detection_period) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL); // Get the current time

    // Calculate the time difference in microseconds
    int time_diff = (current_time.tv_sec - datagram->timestamp.tv_sec) * 1000000 +
                    (current_time.tv_usec - datagram->timestamp.tv_usec);

    return time_diff > detection_period;
}

// Function to add a door registration datagram to the list if not already in the list
void addDoorRegistration(struct door_reg_datagram *new_door, struct door_reg_datagram fail_safe_doors[], int* num_doors) {
    // Check if the new door is already in the list
    for (int i = 0; i < *num_doors; i++) {
        if (fail_safe_doors[i].door_port == new_door->door_port) {
            // The door is already in the list, no need to add it again
            return;
        }
    }

    // If the loop completes and the door is not found, add it to the list
    fail_safe_doors[*num_doors] = new_door;
    (*num_doors)++; // Increment the number of doors in the list
}



int main(int argc, char **argv)
{
    /* Check for error in input arguments */
    if (argc < 9)
    {
        fprintf(stderr, "Missing command line arguments, {address:port} {temperature threshold} {min detections} {detection period (in microseconds)} {reserved argument} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    /* Initialise input arguments */
    char *full_addr_firealarm = argv[1];
    char firealarm_addr[10];
    int firealarm_port = split_Address_Port(full_addr_firealarm, firealarm_addr);
    int temp_threshold = atoi(argv[2]);
    int min_detections = atoi(argv[3]);
    int detection_period = atoi(argv[4]);
    const char *shm_path = argv[6];
    int shm_offset = atoi(argv[7]);
    char overseer_addr;
    int overseer_port = split_Address_Port(argv[8], overseer_addr);
    char buff[512]; // Safe buffer size for UDP communication
    int numDetections = 0;
    int numDoors = 0;

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
    addr.sin_port = htons(firealarm_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the UDP socket*/
    if (bind(recvsockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind()");
        exit(1);
    }

    /* Send socket to send UDP datagram to overseer */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1) {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address of overseer (address + Port) */
    struct sockaddr_in senderaddr;
    (void)memset(&senderaddr, 0, sizeof(senderaddr));
    socklen_t senderaddr_len = sizeof(senderaddr);

    /* Write msg into buf */
    sprintf(buff, "FIREALARM %s:%d HELLO#\n", firealarm_addr, firealarm_port);

    /* Send initialisation message to overseer */
    if (send_message_to_overseer(buff, overseer_port, overseer_addr) == -1)
    {
        perror("send_message_to_overseer()");
        exit(1);
    }
    /* Normal function */

    for(;;)
    {
        // Receives first 4 bytes and stores into buffer
        ssize_t bytes = recvfrom(recvsockfd, buff, 512, 0, (struct sockaddr *)&senderaddr, &senderaddr_len);
        if (bytes == -1)
        {
            perror("recvfrom()");
            exit(1);
        }

        if (strncmp(buff, "FIRE", 4) == 0)
        {
            // fire emergency datagram

        }
        else if (strncmp(buff, "TEMP", 4) == 0)
        {
            // Temp update datagram
            struct temp_datagram_format *pointer = buff;
            if (pointer->temperature < temp_threshold)
            {
                // Loop back to start
                continue;
            }
            else if (isTimestampOld(pointer,detection_period))
            {
                // Loop back to start
                continue;
            }
            // Delete all timestamps older than {detection period} microseconds from the detection list
            removeTimestamps(detections,numDetections, detection_period);
            // Add timestep to detections array 
            if (numDetections < 50)
            {
                detections[numDetections].tv_usec = detection_period;

            }
            else
            {
                perror("Array is full. Cannot add more elements. \n");
                exit(1);
            }

            // Check if there is at least {min detections} entries in the detection list
            if (!(numDetections >= min_detections))
            {
                // Loop to start
                continue;
            }

        }
        else if (strncmp(buff, "DOOR", 4) == 0)
        {
            // Door registration datagram
            struct door_reg_datagram *pointer = buff;
            // If door not in the list add it
            addDoorRegistration(pointer,fail_safe_doors,numDoors);
            


        }
        else
        {
            // error handling
        }






    }








} /* End main */