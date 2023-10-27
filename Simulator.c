#include <stdio.h>
#include <stddef.h>
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
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

/* Global variables */
int cardReaderNum = 0; // Initialise variables to keep count of initialised programs
int doorNum = 0;       // Initialise variables to keep count of initialised programs
int tempsensorNum = 0; // Initialise variables to keep count of initialised programs
int callpointNum = 0;  // Initialise variables to keep count of initialised programs
int portNumber = 3001; // Initial Port Number
struct timeval startTime;
int ifshutdown;

char *shm_path = "/shm"; // shm path of the shared memory
pid_t pidList[102];      // List of process IDs
int pidNum = 0;          // Initialise count of pids

int overseerPort;
int fireAlarmPort;
int doorPort[20];
int temPort[20];

struct door_data
{
    shm_door *door; // Pointer to shm_door
    int door_open_duration;
};

char overseerAddress[17] = "127.0.0.1:3001";

pthread_mutex_t globalMutex;

/* Function Definitions */
void initSharedMemory(char *shm_path, sharedMemory *memory);

void initSharedStructs(char *scenarioName, sharedMemory *memory);

void init(char *scenarioName, sharedMemory *memory);

void handleScenarioLines(char *scenarioName, sharedMemory *memory);

int waitTillTimestamp(struct timeval *startTime, int microseconds_to_wait);

void *handleDoorScenario(void *arg);

int main(int argc, char **argv)
{
    /* Check for error in input arguments */
    if (argc < 1)
    {
        fprintf(stderr, "Missing command line arguments, {scenario file}\n");
        exit(1);
    }

    // Get the start time
    gettimeofday(&startTime, NULL);

    /* Initialise input arguments */
    char *scenarioName = argv[1];

    sharedMemory memory;

    // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(shm_path);

    // This function initialises the shared memory
    initSharedMemory(shm_path, &memory);

    // This functions innitialises the mutex and condvars and the default values in the struct
    initSharedStructs(scenarioName, &memory);

    // Runs all the init programs from the scenario file
    init(scenarioName, &memory);

    // Run all the scenarios under the scenario file
    handleScenarioLines(scenarioName, &memory);

    sleep(5);

    // Kill all process
    // Signal to send for termination (SIGTERM, signal 15)
    for (int i = 0; i < pidNum; i++)
    {
        if (kill(pidList[i], SIGTERM) == 0)
        {
            printf("Termination signal sent to process with PID %d.\n", pidList[i]);
        }
        else
        {
            perror("kill");
        }
    }

    // int serverPort = 3000;
    // char serverAddress[16] = "127.0.0.1";

    // initOverseer(argv[1], serverAddress, serverPort);
    // init(argv[1]);

    return 0;
}

//<summary>
// Function that creates the shared memory
//</summary>
void initSharedMemory(char *shm_path, sharedMemory *memory)
{

    /* Open share memory segment */
    int shm_fd = shm_open(shm_path, O_RDWR | O_CREAT, 0666); // Creates shared memory

    if (shm_fd == -1)
    {
        perror("shm_open()");
        exit(1);
    }

    // Set the capacity of the shared memory object via ftruncate. If the
    // operation fails, ensure that shm->data is NULL and return false.
    if (ftruncate(shm_fd, (off_t)sizeof(sharedMemory)) == -1)
    {
        perror("ftruncate()");
        exit(1);
    }

    memory = mmap(NULL, sizeof(sharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (memory == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }
}

void initSharedStructs(char *scenarioName, sharedMemory *memory)
{
    /* Read in file and confirm success */
    FILE *fhA = fopen(scenarioName, "r");
    char lineA[100]; // Assuming a line won't exceed 100 characters

    /* Check to see if file opened */
    if (fhA == NULL)
    {
        perror("Error opening scenario file");
        return;
    }

    // Create attribute for mutex
    pthread_mutexattr_t mattr;
    // Create attribute for condvar
    pthread_condattr_t cattr;

    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED); // Set mutex attribute to Process shared
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);  // Set condvar attribute to Process shared

    /* Check each line for "INIT, if found find the process and execl() it"*/
    while (fgets(lineA, sizeof(lineA), fhA) != NULL)
    {
        if (strstr(lineA, "INIT"))
        {
            char *pointer;

            char *token = strtok_r(lineA, " ", &pointer); // Split the line by space
            token = strtok_r(NULL, " ", &pointer);        // Split the line by space

            if (strcmp(token, "overseer") == 0)
            {
                pthread_mutex_init(&memory->overseer.mutex, &mattr); // Initialise the mutex in shared memory
                pthread_cond_init(&memory->overseer.cond, &cattr);   // Initialise the condvar in shared memory

                pthread_mutex_lock(&memory->overseer.mutex);   // Lock the mutex
                memory->overseer.security_alarm = '-';         // Set security alarm to '-'
                pthread_mutex_unlock(&memory->overseer.mutex); // Unlock the mutex

                overseerPort = portNumber++; // Keeps track of port number of overseer
            }
            else if (strcmp(token, "cardreader") == 0)
            {

                pthread_mutex_init(&memory->cardreader[cardReaderNum].mutex, &mattr);        // Initialise the mutex in shared memory
                pthread_cond_init(&memory->cardreader[cardReaderNum].response_cond, &cattr); // Initialise the condvar in shared
                pthread_cond_init(&memory->cardreader[cardReaderNum].scanned_cond, &cattr);  // Initialise the condvar in shared

                pthread_mutex_lock(&memory->cardreader[cardReaderNum].mutex);

                /* Debug */
                // strcpy(memory->cardreader[cardReaderNum].scanned, "db4ed0a0bfbb00ac");

                memset(memory->cardreader[cardReaderNum].scanned, '\0', sizeof(memory->cardreader[cardReaderNum].scanned)); // Set scanned array to '\0'
                memory->cardreader->response = '\0'; // set response char to '\0'

                pthread_mutex_unlock(&memory->cardreader[cardReaderNum].mutex);

                cardReaderNum++; // Updates global cardreader count
            }
            else if (strcmp(token, "firealarm") == 0)
            {

                pthread_mutex_init(&memory->firealarm.mutex, &mattr); // Initialise the mutex in shared memory
                pthread_cond_init(&memory->firealarm.cond, &cattr);   // Initialise the condvar in shared memory

                pthread_mutex_lock(&memory->firealarm.mutex);
                memory->firealarm.alarm = '-';
                pthread_mutex_unlock(&memory->firealarm.mutex);

                fireAlarmPort = portNumber++; // Keeps track of port number of firealarm
            }
            else if (strcmp(token, "door") == 0)
            {

                pthread_mutex_init(&memory->doors[doorNum].mutex, &mattr);     // Initialise the mutex in shared memory
                pthread_cond_init(&memory->doors[doorNum].cond_end, &cattr);   // Initialise the condvar in shared memory
                pthread_cond_init(&memory->doors[doorNum].cond_start, &cattr); // Initialise the condvar in shared memory

                pthread_mutex_lock(&memory->doors[doorNum].mutex);
                memory->doors[doorNum].status = 'c';
                pthread_mutex_unlock(&memory->doors[doorNum].mutex);

                doorPort[doorNum++] = portNumber++; // Keeps track of port numbers of each door and increments global doorNum
            }
            else if (strcmp(token, "callpoint") == 0)
            {
                pthread_mutex_init(&memory->callpoint[callpointNum].mutex, &mattr); // Initialise the mutex in shared memory
                pthread_cond_init(&memory->callpoint[callpointNum].cond, &cattr);   // Initialise the condvar in shared memory

                pthread_mutex_lock(&memory->callpoint[callpointNum].mutex);
                memory->callpoint[callpointNum].status = '-';
                pthread_mutex_unlock(&memory->callpoint[callpointNum].mutex);

                callpointNum++; // Updates global callpoint count
            }
            else if (strcmp(token, "tempsensor") == 0)
            {
                pthread_mutex_init(&memory->tempsensor[tempsensorNum].mutex, &mattr); // Initialise the mutex in shared memory
                pthread_cond_init(&memory->tempsensor[tempsensorNum].cond, &cattr);   // Initialise the condvar in shared memory

                pthread_mutex_lock(&memory->tempsensor[tempsensorNum].mutex);
                memory->tempsensor[tempsensorNum].temperature = 22.0;
                pthread_mutex_unlock(&memory->tempsensor[tempsensorNum].mutex);

                temPort[tempsensorNum++] = portNumber++; // Keeps track of port numbers of tempsensors and updates global port number
            }
            else
            {
                fprintf(stderr, "Invalid component in scenario file");
                exit(1);
            }
        }
        else
        {
            break;
        }
    }
    fclose(fhA);
    // Can destroy the condition attribute as it's no longer needed
    pthread_condattr_destroy(&cattr);
    pthread_mutexattr_destroy(&mattr);
}

/// <summary>
/// Function takes input of scenario file name and runs all initialisation setup
/// commands. Returns no value. Errors if scenario file is not valid.
/// </summary>
void init(char *scenarioName, sharedMemory *memory)
{
    char *serverAddress = "127.0.0.1";

    /* Read in file and confirm success */
    FILE *fhA = fopen(scenarioName, "r");
    char lineA[100]; // Assuming a line won't exceed 100 characters
    char lineCompare[100];
    // int currentPort = 3000; // Starting port number

    /* Check to see if file opened */
    if (fhA == NULL)
    {
        perror("Error opening scenario file");
        return;
    }

    // Variables to keep track of how many components initialised
    int doorCount = 0;
    int cardreaderCount = 0;
    int callpointCount = 0;
    /* Check each line for "INIT, if found find the process and execl() it"*/
    while (fgets(lineA, sizeof(lineA), fhA) != NULL)
    {
        strcpy(lineCompare, lineA);
        if (strstr(lineA, "INIT"))
        {
            char *pointer;
            char *token = strtok_r(lineCompare, " ", &pointer); // Split the line by space
            token = strtok_r(NULL, " ", &pointer);

            /* Create child process */
            pid_t child_pid = fork();

            /* Confirm successful creation of child process */
            if (child_pid == -1)
            {
                perror("Fork failed");
                return;
            }
            /* Parent process to become overseer */
            else if (child_pid > 0)
            {
                if (strcmp(token, "overseer") == 0)
                {

                    ssize_t offset = offsetof(sharedMemory, overseer); // Calculates the offset for overseer in shared memory

                    char shm_offset[256]; // char buffer for storing the offset

                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset); // Convert offset to string

                    char door_open_duration[64], datagram_resend_delay[64], authorisation_file[64], connection_file[64], layout_file[64]; // To grab input argument from text file

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT overseer %s %s %s %s %s", door_open_duration, datagram_resend_delay, authorisation_file, connection_file, layout_file) != 5)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    // snprintf(overseerAddress, 17, "%s:%d", serverAddress, overseerPort);

                    char *args[] = {"./overseer", overseerAddress, door_open_duration, datagram_resend_delay, authorisation_file, connection_file, layout_file, shm_path, shm_offset, NULL};

                    // Launches overseer in the parent
                    execv("overseer", args);

                    perror("execv");
                }
                else if (!strcmp(token, "door"))
                {
                    char id[64], config[64], doorOpenDelay[64];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT door %s %s %s", id, config, doorOpenDelay) != 3)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    // Scenario for door handled in here
                    // Initialise a thread to handle Door Scenario file
                    pthread_t doorThread;

                    struct door_data *doorData = (struct door_data *)malloc(sizeof(struct door_data)); // Allocate memory to the pointer

                    doorData->door = &memory->doors[doorCount]; // Pointer to the door initialised
                    doorData->door_open_duration = atoi(doorOpenDelay);

                    if (pthread_create(&doorThread, NULL, handleDoorScenario, doorData) != 0)
                    {
                        perror("pthread_create()");
                        exit(1);
                    }

                    doorCount++;                   // increments doorCount
                    pidList[pidNum++] = child_pid; // Add child pid to pid list
                }
                else if (!strcmp(token, "cardreader"))
                {
                    cardreaderCount++;
                    pidList[pidNum++] = child_pid; // Add child pid to pid list
                }
                else if (!strcmp(token, "firealarm"))
                {
                    pidList[pidNum++] = child_pid; // Add child pid to pid list
                }
                else if (!strcmp(token, "callpoint"))
                {
                    callpointCount++;
                    pidList[pidNum++] = child_pid; // Add child pid to pid list
                }
                else if (!strcmp(token, "tempsensor"))
                {
                    // printf("tempsensor found in line %s\n", lineA);
                }
            }
            else if (child_pid == 0) // Child process
            {
                usleep(250000); // Child sleeps for 250,000 microseconds, which is 250 milliseconds
                if (!strcmp(token, "overseer"))
                {
                    pid_t parent_pid = getppid(); // Gets parents process
                    pidList[pidNum++] = parent_pid;
                }
                if (!strcmp(token, "door"))
                {

                    ssize_t offset = offsetof(sharedMemory, doors) + (doorCount * sizeof(shm_door));

                    char shm_offset[256]; // char buffer for storing the offset

                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset); // Convert offset to string

                    char id[64], config[64], doorOpenDelay[64], doorAddress[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT door %s %s %s", id, config, doorOpenDelay) != 3)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(doorAddress, 17, "%s:%d", serverAddress, doorPort[doorCount]);

                    // printf("%s %s %s %s %s %s\n", id, doorAddress, config, shm_path, shm_offset, overseerAddress);

                    execl("door", "./door", id, doorAddress, config, shm_path, shm_offset, overseerAddress, NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "cardreader"))
                {

                    ssize_t offset = offsetof(sharedMemory, cardreader) + (cardreaderCount * sizeof(shm_cardreader));

                    char shm_offset[256]; // char buffer for storing the offset

                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset); // Convert offset to string

                    char id[64], waitTime[64];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT cardreader %s %s ", id, waitTime) != 2)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    // printf("%s %s %s %s %s\n", id, waitTime, shm_path, shm_offset, overseerAddress);

                    execl("cardreader", "./cardreader", id, waitTime, shm_path, shm_offset, overseerAddress, NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "firealarm"))
                {

                    ssize_t offset = offsetof(sharedMemory, firealarm);

                    char shm_offset[256]; // char buffer for storing the offset

                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset); // Convert offset to string

                    char tempThreshold[64], minDetections[64], detectionPeriod[64], reserved[64], fireAlarmAddr[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT firealarm %s %s %s %s", tempThreshold, tempThreshold, detectionPeriod, reserved) != 4)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(fireAlarmAddr, 17, "%s:%d", serverAddress, fireAlarmPort);

                    // printf("%s %s %s %s %s %s %s %s\n", fireAlarmAddr, tempThreshold, minDetections, detectionPeriod, reserved, shm_path, shm_offset, overseerAddress);

                    char *args[] = {"./firealarm", fireAlarmAddr, tempThreshold, minDetections, detectionPeriod, reserved, shm_path, shm_offset, overseerAddress, NULL};

                    execv("firealarm", args);
                    perror("execl");
                }
                else if (!strcmp(token, "callpoint"))
                {

                    ssize_t offset = offsetof(sharedMemory, callpoint) + (cardreaderCount * sizeof(shm_callpoint));

                    char shm_offset[256]; // char buffer for storing the offset

                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset); // Convert offset to string

                    char reserved[64], waitTime[64], fireAlarmAddr[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT callpoint %s %s", waitTime, reserved) != 2)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(fireAlarmAddr, 17, "%s:%d", serverAddress, fireAlarmPort);

                    // printf("%s %s %s %s\n", waitTime, shm_path, shm_offset, fireAlarmAddr);

                    execl("callpoint", "./callpoint", waitTime, shm_path, shm_offset, fireAlarmAddr, NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "tempsensor"))
                {
                    // printf("tempsensor found in line %s\n", lineA);
                }
            }
        }
        else
        {
            break; // All innit lines over
        }
    }
    fclose(fhA);
}
//<summary>
// Function handles Scenario section
//</summary>
void handleScenarioLines(char *scenarioName, sharedMemory *memory)
{
    FILE *fhA = fopen(scenarioName, "r");
    char lineA[100];

    if (fhA == NULL)
    {
        perror("Error opening scenario file");
        return;
    }

    int foundScenario = 0;

    while (fgets(lineA, sizeof(lineA), fhA) != NULL)
    {
        if (foundScenario)
        {
            // Process the lines after "SCENARIO"
            // For example, you can print them:
            // printf("Line after SCENARIO: %s\n", lineA);
            if (strstr(lineA, "CARD_SCAN"))
            {
                // printf("Cardscan line is %s\n", lineA);

                char timestamp[64], num[64], code[64];
                /* Check that sscanf is successful */
                if (sscanf(lineA, "%s CARD_SCAN %s %s", timestamp, num, code) != 3)
                {
                    perror("sscanf failed");
                    exit(1);
                }

                int timeStamp = atoi(timestamp);
                int cardReaderIndex = atoi(num);

                if (waitTillTimestamp(&startTime, timeStamp) == 0)
                {
                    fprintf(stderr, "waitTillTimestamp - Varibles defined inproperly\n");
                    exit(1);
                }
                else
                {
                    pthread_mutex_lock(&memory->cardreader[cardReaderIndex].mutex);
                    memcpy(memory->cardreader[cardReaderIndex].scanned, code, 16);
                    // printf("Scanned hash from card reader is %s\n",memory->cardreader[cardReaderIndex].scanned);
                    pthread_mutex_unlock(&memory->cardreader[cardReaderIndex].mutex);
                    // pthread_cond_signal(&memory->cardreader[cardReaderIndex].scanned_cond);
                }
            }
            else if (strstr(lineA, "CALLPOINT_TRIGGER"))
            {
            }
            else if (strstr(lineA, "TEMP_CHANGE"))
            {
            }
        }
        else if (strstr(lineA, "SCENARIO"))
        {
            foundScenario = 1;
        }
    }

    fclose(fhA);
}

//<summary>
// Function sleeps till timestamp returns a 0 if time to wait already elapsed
//</summary>
int waitTillTimestamp(struct timeval *startTime, int microseconds_to_wait)
{
    struct timeval current_time;

    // Start time in microsec
    long startTime_microsec = startTime->tv_sec * 1000000 + startTime->tv_usec;

    // Get the current time
    gettimeofday(&current_time, NULL);

    // Current time in micro secs
    long currentTime_microsec = current_time.tv_sec * 1000000 + current_time.tv_usec;

    long timeElapsed_microsec = currentTime_microsec - startTime_microsec;

    if (timeElapsed_microsec < microseconds_to_wait)
    {
        long remaining_microseconds = microseconds_to_wait - timeElapsed_microsec;

        // Sleep for the remaining time
        usleep(remaining_microseconds);
        return 1;
    }
    else
    {
        return 1; // Dont wait
    }
}

// //<summary>
// // Thread function to handle the door scenario
// //</summary>
void *handleDoorScenario(void *arg)
{
    struct door_data doorData = *(struct door_data *)arg;
    printf("Im in the door thread\n");

    // Lock the mutex to protect shared data
    pthread_mutex_lock(&doorData.door->mutex);
    while (!(ifshutdown))
    {
        // Wait on cond_start
        pthread_cond_wait(&doorData.door->cond_start, &doorData.door->mutex);

        printf("Cond start signaled\n");

        // Upon waking up, check the status
        if (doorData.door->status == 'O' || doorData.door->status == 'C')
        {
            printf("Door status is O or C\n");
            // Wait on cond_start again
            pthread_cond_wait(&doorData.door->cond_start, &doorData.door->mutex);
        }
        else
        {
            printf("Door status is not O or C\n");

            // Sleep for {door open/close time} microseconds
            usleep(doorData.door_open_duration);

            // Change the status
            if (doorData.door->status == 'o')
            {
                doorData.door->status = 'O';
            }
            else if (doorData.door->status == 'c')
            {
                doorData.door->status = 'C';
            }
            printf("Cond end signaled\n");
            // Signal cond_end to notify others that the operation is complete
            pthread_cond_signal(&doorData.door->cond_end);
        }
    }
    free(arg);
    return NULL;
}
