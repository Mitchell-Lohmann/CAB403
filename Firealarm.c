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

struct door_confirm_datagram{
    char header[4]; // {'D', 'R', 'E', 'G'}
    struct in_addr door_addr;
    in_port_t door_port;
};


// The 'min detections' value will be <= 50 (which means the detections list only needs to be 50 entries large)
struct timeval detections[50];

// There will be <= 100 fail-safe doors, which means the fail-safe doors list only needs to be 100 entries large
struct door_reg_datagram fail_safe_doors[100];

/* Function initialisation */

void removeOldTimestamps(struct timeval* detections, int* numDetections, int detection_period);

int isTimestampOld(struct temp_datagram_format *datagram, int detection_period) ;

int isNewDoor(struct door_reg_datagram *new_door, struct door_reg_datagram fail_safe_doors[], int* num_doors);

int setfireAlarm(shm_firealarm *shm, struct door_reg_datagram *doors ,int numDoors);

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
    char *full_addr_firealarm = argv[1];
    char firealarm_addr[10];
    int firealarm_port = split_Address_Port(full_addr_firealarm, firealarm_addr);
    int temp_threshold = atoi(argv[2]);
    int min_detections = atoi(argv[3]);
    int detection_period = atoi(argv[4]);
    const char *shm_path = argv[6];
    int shm_offset = atoi(argv[7]);
    char overseer_addr[10];
    int overseer_port = split_Address_Port(argv[8], overseer_addr);
    char buff[512]; // Safe buffer size for UDP communication
    int numDetections = 0;
    int numDoors = 0;
    int ifFireAlarmSet = 0; // variable to track if fire alarm is set

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

    /* Create send socket to send UDP datagram to overseer */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1) {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address of overseer (address + Port) */
    struct sockaddr_in sendtoaddr;
    (void)memset(&sendtoaddr, 0, sizeof(sendtoaddr));
    sendtoaddr.sin_family = AF_INET;
    sendtoaddr.sin_port = htons(overseer_port);
    if (inet_pton(AF_INET, overseer_addr, &sendtoaddr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", overseer_addr);
        exit(1);
    }
    socklen_t senderaddr_len = sizeof(sendtoaddr);

    /* Write msg into buf */
    sprintf(buff, "FIREALARM %s:%d HELLO#\n", firealarm_addr, firealarm_port);

    /* Send initialisation message to overseer */
    if (send_message_to(buff, overseer_port, overseer_addr, 1) == -1)
    {
        perror("send_message_to()");
        exit(1);
    }

    /* Open share memory segment */
    int shm_fd = shm_open(shm_path, O_RDWR, 0666); // Creating for testing purposes
    
    if(shm_fd == -1){
        perror("shm_open()");
        exit(1);
    }
    
    /*fstat helps to get information of the shared memory like its size*/
    struct stat shm_stat;
    if(fstat(shm_fd, &shm_stat) == -1)
    {
        perror("fstat()");
        exit(1);
    }

    char *shm = mmap(NULL, shm_stat.st_size,PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    /* shared is used to access the shared memory */
    shm_firealarm *shared = (shm_firealarm *)(shm + shm_offset);

    /* Normal function */

    for(;;)
    {
        // Receives first 4 bytes and stores into buffer
        ssize_t bytes = recvfrom(recvsockfd, buff, 512, 0, NULL, NULL);
        if (bytes == -1)
        {
            perror("recvfrom()");
            exit(1);
        }

        if ((strncmp(buff, "FIRE", 4) == 0) && !ifFireAlarmSet)
        {
            // fire emergency datagram
            ifFireAlarmSet = setfireAlarm(shared, fail_safe_doors, numDoors);

        }
        else if ((strncmp(buff, "TEMP", 4) == 0) && !ifFireAlarmSet)
        {
            // Temp update datagram
            struct temp_datagram_format *pointer = (struct temp_datagram_format *)buff;
            if (pointer == NULL)
            {
                printf("Pointer points to NULL");
                exit(1);
            }
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
            removeOldTimestamps(detections,&numDetections, detection_period);
            // Add timestep to detections array 
            if (numDetections < 50)
            {
                // Add timestamp to detections array
                detections[numDetections] = pointer->timestamp;
                numDetections++;
            }
            else
            {
                perror("Array is full. Cannot add more elements. \n");
                exit(1);
            }

            // Check if there is at least {min detections} entries in the detection list
            if (numDetections >= min_detections)
            {
                
                ifFireAlarmSet =  setfireAlarm(shared, fail_safe_doors, numDoors);
            }
            else
            {
                // Loop to start
                continue;
            }

        }
        else if ((strncmp(buff, "DOOR", 4) == 0) && !ifFireAlarmSet)
        {
            // Door registration datagram
            struct door_reg_datagram *pointer = (struct door_reg_datagram *)buff;
            if (pointer == NULL)
            {
                printf("Pointer points to NULL");
                exit(1);
            }

            // If door not in the list add it
            if (isNewDoor(pointer, fail_safe_doors, &numDoors))
            {
                if(numDoors < 100) // checks if max number of allowable doors reached
                {
                    fail_safe_doors[numDoors] = *pointer; // Dereference pointer to access the data
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
                // Loop to start 
                continue; 
            }

            /* Create an instance of DREG datagram */
            struct door_confirm_datagram confirm_door = initialise_DREG_Struct(pointer);

            // Send door confirmation datagram to overseer
            (void)sendto(sendfd, &confirm_door, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, senderaddr_len);

            // Loop to start 
            continue; 

        }
        else if ((strncmp(buff, "DOOR", 4) == 0) && ifFireAlarmSet)
        {
            // Door registration datagram
            struct door_reg_datagram *pointer = (struct door_reg_datagram *)buff;
            if (pointer == NULL)
            {
                printf("Pointer points to NULL");
                exit(1);
            }

            char* buf = "OPEN_EMERG#";

            // Convert the in_addr to a string
            char *addr_str = inet_ntoa(pointer->door_addr);


            // Send OPEN_EMERG# to newly registered door
            send_message_to(buf, pointer->door_port, addr_str, 1);

            /* Create an instance of DREG datagram */
            struct door_confirm_datagram confirm_door = initialise_DREG_Struct(pointer);

            // Send door confirmation datagram to overseer
            (void)sendto(sendfd, &confirm_door, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, senderaddr_len);

            // Loop to start 
            continue; 
        }
    }

} /* End main */


/* Function Definitions */

// Function to remove old timestamps
void removeOldTimestamps(struct timeval* detections, int* numDetections, int detection_period) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL); // Get the current time

    int numValidDetections = 0;
    
    for (int i = 0; i < *numDetections; i++) {

        int time_elapsed = (current_time.tv_sec - detections[i].tv_sec) * 1000000 +
                    (current_time.tv_usec - detections[i].tv_usec);
        
        // Check if the time difference is less than the detection period
        if (time_elapsed < detection_period) {
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
    int time_elapsed = (current_time.tv_sec - datagram->timestamp.tv_sec) * 1000000 +
                    (current_time.tv_usec - datagram->timestamp.tv_usec);

    return time_elapsed > detection_period;
}

// Function to check if the Door is present in the door list
int isNewDoor(struct door_reg_datagram *new_door, struct door_reg_datagram fail_safe_doors[], int* num_doors) {
    // Check if the new door is already in the list
    for (int i = 0; i < *num_doors; i++) {
        if (htonl(fail_safe_doors[i].door_port) == htonl(new_door->door_port)) {
            // The door is already in the list, no need to add it again
            return 0;
        }
    }
    return 1;
}


// Function that sets fire alarm in shared memory and sends OPEN_EMERG# to every fail_safe doors
int setfireAlarm(shm_firealarm *shm, struct door_reg_datagram doors[] ,int numDoors)
{
    char *buff = "OPEN_EMERG#";

    pthread_mutex_lock(&shm->mutex);
    shm->alarm = 'A';
    pthread_mutex_unlock(&shm->mutex);
    pthread_cond_signal(&shm->cond);

    for (int i = 0; i < numDoors; i++)
    {
        // Convert in_port_t to int
        int doorPort = htons(doors[i].door_port);
        // Convert the in_addr to a string
        char *addr_str = inet_ntoa(doors[i].door_addr);
        if (send_message_to(buff, doorPort, addr_str, 1) == -1)
        {
            perror("send_message_to()");
            exit(1);
        }

    }
    return 1;

}

struct door_confirm_datagram initialise_DREG_Struct(struct door_reg_datagram *door)
{
    struct door_confirm_datagram new_door;
    // Write DREG to header
    (void)memcpy(new_door.header, "DREG", sizeof(new_door.header));
    // Write addr and port of door to DREG datagram
    new_door.door_addr = door->door_addr;
    new_door.door_port = door->door_port;

    return new_door;

}




    