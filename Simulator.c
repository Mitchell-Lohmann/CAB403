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
/* Initialise variables to keep count of initialised programs */
int cardReaderNum = 0;
int doorNum = 0;
int tempsensorNum = 0;
int callpointNum = 0;
/* Initial port number */
int portNumber = 3000;
struct timeval startTime;
int ifshutdown;

/* shm path of the shared memory */
char *shm_path = "/shm";
/* List of process IDs */
pid_t pidList[102];
/* Initialise count of pids */
int pidNum = 0;
int overseerPort;
int fireAlarmPort;
int doorPort[20];
int temPort[20];

struct door_data
{
    /* Pointer to shm_door */
    shm_door *door;
    int door_open_duration;
};

char overseerAddress[17] = "127.0.0.1:3000";

pthread_mutex_t globalMutex;

/* Function Definitions */
void initSharedMemory(char *shm_path, sharedMemory **memory);

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

    /* Initialise input arguments */
    char *scenarioName = argv[1];

    sharedMemory *memory;

    /* Remove any previous instance of the shared memory object, if it exists. */
    shm_unlink(shm_path);

    /* This function initialises the shared memory */
    initSharedMemory(shm_path, &memory);

    /* This functions innitialises the mutex and condvars and the default values in the struct */
    initSharedStructs(scenarioName, memory);

    /* Runs all the init programs from the scenario file */
    init(scenarioName, memory);

    sleep(1);

    /* Start simulating events */

    /* Get the start time for simulating events */
    gettimeofday(&startTime, NULL);

    /* Run all the scenarios under the scenario file */
    handleScenarioLines(scenarioName, memory);

    sleep(1);
    usleep(130000);

    if (ifshutdown)
    {
        /* Kill all process */
        /* Signal to send for termination (SIGTERM, signal 15) */
        for (int i = 0; i < pidNum; i++)
        {
            if (kill(pidList[i], SIGTERM) != 0)
            {
                perror("kill");
                exit(1);
            }
        }
        printf("Gracefully shutting down all programs\n");
    }
    else /* Something went wrong forecekill every process */
    {
        for (int i = 0; i < pidNum; i++)
        {
            if (kill(pidList[i], SIGKILL) != 0)
            {
                perror("kill");
                exit(1);
            }
        }
    }


    return 0;
} /* End main */

//<summary>
// Function that creates the shared memory.
//</summary>
void initSharedMemory(char *shm_path, sharedMemory **memory)
{

    /* Open and create shared memory segment */
    int shm_fd = shm_open(shm_path, O_RDWR | O_CREAT, 0666);

    if (shm_fd == -1)
    {
        perror("shm_open()");
        exit(1);
    }

    /* Set the capacity of the shared memory object via ftruncate. If the */
    /* operation fails, ensure that shm->data is NULL and return false. */
    if (ftruncate(shm_fd, (off_t)sizeof(sharedMemory)) == -1)
    {
        perror("ftruncate()");
        exit(1);
    }

    *memory = mmap(NULL, sizeof(sharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (*memory == MAP_FAILED)
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

    /* Create attribute for mutex */
    pthread_mutexattr_t mattr;
    /* Create attribute for condvar */
    pthread_condattr_t cattr;

    /* Set mutex attribute to Process shared */
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    /* Set condvar attribute to Process shared */
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    /* Check each line for "INIT, if found find the process and execl() it"*/
    while (fgets(lineA, sizeof(lineA), fhA) != NULL)
    {
        if (strstr(lineA, "INIT"))
        {
            char *pointer;

            char *token = strtok_r(lineA, " ", &pointer);
            token = strtok_r(NULL, " ", &pointer);

            if (strcmp(token, "overseer") == 0)
            {
                /* Initialise the mutex and condvar in shared memory */
                pthread_mutex_init(&memory->overseer.mutex, &mattr);
                pthread_cond_init(&memory->overseer.cond, &cattr);

                /* Lock the mutex, Set security alarm to '-', Unlock the mutex" */
                pthread_mutex_lock(&memory->overseer.mutex);
                memory->overseer.security_alarm = '-';
                pthread_mutex_unlock(&memory->overseer.mutex);

                /* Keeps track of port number of overseer */
                overseerPort = portNumber++;
            }
            else if (strcmp(token, "cardreader") == 0)
            {

                pthread_mutex_init(&memory->cardreader[cardReaderNum].mutex, &mattr);
                pthread_cond_init(&memory->cardreader[cardReaderNum].response_cond, &cattr);
                pthread_cond_init(&memory->cardreader[cardReaderNum].scanned_cond, &cattr);

                pthread_mutex_lock(&memory->cardreader[cardReaderNum].mutex);

                /* Set scanned array to '\0', set response char to '\0'*/
                memset(memory->cardreader[cardReaderNum].scanned, '\0', sizeof(memory->cardreader[cardReaderNum].scanned));
                memory->cardreader->response = '\0';

                pthread_mutex_unlock(&memory->cardreader[cardReaderNum].mutex);

                /* Updates global cardreader count */
                cardReaderNum++;
            }
            else if (strcmp(token, "firealarm") == 0)
            {
                /* Initialise the mutex and condvar in shared memory */
                pthread_mutex_init(&memory->firealarm.mutex, &mattr);
                pthread_cond_init(&memory->firealarm.cond, &cattr);

                pthread_mutex_lock(&memory->firealarm.mutex);
                memory->firealarm.alarm = '-';
                pthread_mutex_unlock(&memory->firealarm.mutex);

                /* Keeps track of port number of firealarm */
                fireAlarmPort = portNumber++;
            }
            else if (strcmp(token, "door") == 0)
            {
                /* Initialise the mutex and condvar in shared memory */
                pthread_mutex_init(&memory->doors[doorNum].mutex, &mattr);
                pthread_cond_init(&memory->doors[doorNum].cond_end, &cattr);
                pthread_cond_init(&memory->doors[doorNum].cond_start, &cattr);

                pthread_mutex_lock(&memory->doors[doorNum].mutex);
                memory->doors[doorNum].status = 'C';
                pthread_mutex_unlock(&memory->doors[doorNum].mutex);

                /* Keeps track of port numbers of each door and increments global doorNum */
                doorPort[doorNum++] = portNumber++;
            }
            else if (strcmp(token, "callpoint") == 0)
            {
                /* Initialise the mutex and condvar in shared memory */
                pthread_mutex_init(&memory->callpoint[callpointNum].mutex, &mattr);
                pthread_cond_init(&memory->callpoint[callpointNum].cond, &cattr);

                pthread_mutex_lock(&memory->callpoint[callpointNum].mutex);
                memory->callpoint[callpointNum].status = '-';
                pthread_mutex_unlock(&memory->callpoint[callpointNum].mutex);

                /* Updates global callpoint count */
                callpointNum++;
            }
            else if (strcmp(token, "tempsensor") == 0)
            {
                /* Initialise the mutex and condvar in shared memory */
                pthread_mutex_init(&memory->tempsensor[tempsensorNum].mutex, &mattr);
                pthread_cond_init(&memory->tempsensor[tempsensorNum].cond, &cattr);

                pthread_mutex_lock(&memory->tempsensor[tempsensorNum].mutex);
                memory->tempsensor[tempsensorNum].temperature = 22.0;
                pthread_mutex_unlock(&memory->tempsensor[tempsensorNum].mutex);

                /* Keeps track of port numbers of tempsensors and updates global port number */
                temPort[tempsensorNum++] = portNumber++;
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
    /* Can destroy the condition attribute as it's no longer needed */
    pthread_condattr_destroy(&cattr);
    pthread_mutexattr_destroy(&mattr);
}
/* End main */

    // <summary>
    // Function takes input of scenario file name and runs all initialisation setup
    // commands. Returns no value. Errors if scenario file is not valid.
    // </summary>
    void init(char *scenarioName, sharedMemory *memory)
{
    char *serverAddress = "127.0.0.1";

    /* Read in file and confirm success */
    FILE *fhA = fopen(scenarioName, "r");
    char lineA[100]; // Assuming a line won't exceed 100 characters
    char lineCompare[100];
    /* int currentPort = 3000; // Starting port number */

    /* Check to see if file opened */
    if (fhA == NULL)
    {
        perror("Error opening scenario file");
        return;
    }

    /* Variables to keep track of how many components initialised */
    int doorCount = 0;
    int cardreaderCount = 0;
    int callpointCount = 0;
    int tempSensorCount = 0;
    /* Check each line for "INIT, if found find the process and execl() it"*/
    while (fgets(lineA, sizeof(lineA), fhA) != NULL)
    {
        strcpy(lineCompare, lineA);
        if (strstr(lineA, "INIT"))
        {
            char *pointer;
            char *token = strtok_r(lineCompare, " ", &pointer);
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
                    /* Calculates the offset for overseer in shared memory */
                    ssize_t offset = offsetof(sharedMemory, overseer);

                    /* char buffer for storing the offset */
                    char shm_offset[256];

                    /* Convert offset to string */
                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset);

                    /* To grab input argument from text file */
                    char door_open_duration[64], datagram_resend_delay[64], authorisation_file[64], connection_file[64], layout_file[64];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT overseer %s %s %s %s %s", door_open_duration, datagram_resend_delay, authorisation_file, connection_file, layout_file) != 5)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    /* snprintf(overseerAddress, 17, "%s:%d", serverAddress, overseerPort); */

                    char *args[] = {"./overseer", overseerAddress, door_open_duration, datagram_resend_delay, authorisation_file, connection_file, layout_file, shm_path, shm_offset, NULL};

                    /* Launches overseer in the parent */
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

                    /* Scenario for door handled in here */
                    /* Initialise a thread to handle Door Scenario file */
                    pthread_t doorThread;

                    /* Allocate memory to the pointer */
                    struct door_data *doorData = (struct door_data *)malloc(sizeof(struct door_data));

                    /* Pointer to the door initialised */
                    doorData->door = &memory->doors[doorCount];
                    doorData->door_open_duration = atoi(doorOpenDelay);

                    if (pthread_create(&doorThread, NULL, handleDoorScenario, doorData) != 0)
                    {
                        perror("pthread_create()");
                        exit(1);
                    }

                    /* increments doorCount */
                    doorCount++;
                    /* Add child pid to pid list */
                    pidList[pidNum++] = child_pid;
                }
                else if (!strcmp(token, "cardreader"))
                {
                    cardreaderCount++;
                    /* Add child pid to pid list */
                    pidList[pidNum++] = child_pid;
                }
                else if (!strcmp(token, "firealarm"))
                {
                    /* Add child pid to pid list */
                    pidList[pidNum++] = child_pid;
                }
                else if (!strcmp(token, "callpoint"))
                {
                    callpointCount++;
                    /* Add child pid to pid list */
                    pidList[pidNum++] = child_pid;
                }
                else if (!strcmp(token, "tempsensor"))
                {
                    tempSensorCount++;
                    pidList[pidNum++] = child_pid;
                }
            }
            /* Child process */
            else if (child_pid == 0)
            {
                /* Child sleeps for 250,000 microseconds, which is 250 milliseconds */
                usleep(250000);
                if (!strcmp(token, "overseer"))
                {
                    /* Gets parents process */
                    pid_t parent_pid = getppid();
                    pidList[pidNum++] = parent_pid;
                }
                if (!strcmp(token, "door"))
                {

                    ssize_t offset = offsetof(sharedMemory, doors) + (doorCount * sizeof(shm_door));

                    /* char buffer for storing the offset */
                    char shm_offset[256];

                    /* Convert offset to string */
                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset);

                    char id[64], config[64], doorOpenDelay[64], doorAddress[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT door %s %s %s", id, config, doorOpenDelay) != 3)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(doorAddress, 17, "%s:%d", serverAddress, doorPort[doorCount]);

                    execl("door", "./door", id, doorAddress, config, shm_path, shm_offset, overseerAddress, NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "cardreader"))
                {

                    ssize_t offset = offsetof(sharedMemory, cardreader) + (cardreaderCount * sizeof(shm_cardreader));

                    /* char buffer for storing the offset */
                    char shm_offset[256];

                    /* Convert offset to string */
                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset);

                    char id[64], waitTime[64];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT cardreader %s %s ", id, waitTime) != 2)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    execl("cardreader", "./cardreader", id, waitTime, shm_path, shm_offset, overseerAddress, NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "firealarm"))
                {

                    ssize_t offset = offsetof(sharedMemory, firealarm);

                    /* char buffer for storing the offset */
                    char shm_offset[256];

                    /* Convert offset to string */
                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset);

                    char tempThreshold[64], minDetections[64], detectionPeriod[64], reserved[64], fireAlarmAddr[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT firealarm %s %s %s %s", tempThreshold, minDetections, detectionPeriod, reserved) != 4)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(fireAlarmAddr, 17, "%s:%d", serverAddress, fireAlarmPort);

                    char *args[] = {"./firealarm", fireAlarmAddr, tempThreshold, minDetections, detectionPeriod, reserved, shm_path, shm_offset, overseerAddress, NULL};

                    execv("firealarm", args);
                    perror("execl");
                }
                else if (!strcmp(token, "callpoint"))
                {

                    ssize_t offset = offsetof(sharedMemory, callpoint) + (callpointCount * sizeof(shm_callpoint));

                    /* char buffer for storing the offset */
                    char shm_offset[256];

                    /* Convert offset to string */
                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset);

                    char reserved[64], waitTime[64], fireAlarmAddr[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT callpoint %s %s", waitTime, reserved) != 2)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(fireAlarmAddr, 17, "%s:%d", serverAddress, fireAlarmPort);

                    execl("callpoint", "./callpoint", waitTime, shm_path, shm_offset, fireAlarmAddr, NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "tempsensor"))
                {
                    ssize_t offset = offsetof(sharedMemory, tempsensor) + (tempSensorCount * sizeof(shm_tempsensor));

                    /* char buffer for storing the offset */
                    char shm_offset[256];

                    /* Convert offset to string */
                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset);

                    char id[64], maxCondvarWait[64], maxUpdateWait[64], receiverList[64], thisTempsensorAddr[17];

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT tempsensor %s %s %s %[^/n]", id, maxCondvarWait, maxUpdateWait, receiverList) != 4)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    /* Create {addrss:port} for this temp sensor */

                    snprintf(thisTempsensorAddr, 17, "%s:%d", serverAddress, temPort[tempSensorCount]);

                    char *argv[] = {"./tempsensor", id, thisTempsensorAddr, maxCondvarWait, maxUpdateWait, shm_path, shm_offset, NULL, NULL, NULL, NULL, NULL};

                    /* Create {address:port} for the every receiver in the receivers list */

                    char *token, *saveptr;
                    token = strtok_r(receiverList, " ", &saveptr);
                    int receiverCount = 0;
                    while (token != NULL)
                    {
                        /* Add overseer addr to argument list */
                        if (token[0] == 'O')
                        {
                            argv[receiverCount + 7] = overseerAddress;
                        }
                        /* Add firealarm addr to argument list */
                        else if (token[0] == 'F')
                        {
                            ;
                            /* Create address for fire alarm */
                            char fireAlarmAddr[17];
                            snprintf(fireAlarmAddr, 17, "%s:%d", serverAddress, fireAlarmPort);

                            argv[receiverCount + 7] = fireAlarmAddr;
                        }
                        /* Token is either S1,S2,S3..... */
                        else if (token[0] == 'S')
                        {
                            int tempsensorNo = atoi(token + 1);
                            char receiverTempsensorAddr[17];
                            /* Grabs port from temPort list and make address of receiver */
                            snprintf(receiverTempsensorAddr, 17, "%s:%d", serverAddress, temPort[tempsensorNo]);

                            argv[receiverCount + 7] = strdup(receiverTempsensorAddr);
                        }
                        token = strtok_r(NULL, " ", &saveptr);
                        receiverCount++;
                    }
                    execv("tempsensor", argv);
                    perror("execv");
                }
            }
        }
        else
        {
            break;
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
                    pthread_mutex_unlock(&memory->cardreader[cardReaderIndex].mutex);
                    pthread_cond_signal(&memory->cardreader[cardReaderIndex].scanned_cond);
                }
            }
            else if (strstr(lineA, "CALLPOINT_TRIGGER"))
            {
                char timestamp[64], num[64];
                /* Check that sscanf is successful */
                if (sscanf(lineA, "%s CALLPOINT_TRIGGER %s", timestamp, num) != 2)
                {
                    perror("sscanf failed");
                    exit(1);
                }
                int timeStamp = atoi(timestamp);
                int callpointIndex = atoi(num);
                if (waitTillTimestamp(&startTime, timeStamp) == 0)
                {
                    fprintf(stderr, "waitTillTimestamp - Varibles defined inproperly\n");
                    exit(1);
                }
                else
                {
                    pthread_mutex_lock(&memory->callpoint[callpointIndex].mutex);
                    memory->callpoint[callpointIndex].status = '*';
                    pthread_mutex_unlock(&memory->callpoint[callpointIndex].mutex);
                    pthread_cond_signal(&memory->callpoint[callpointIndex].cond);
                }
            }
            else if (strstr(lineA, "TEMP_CHANGE"))
            {
                char timestamp[64], num[64], newTemp[64];
                /* Check that sscanf is successful */
                if (sscanf(lineA, "%s TEMP_CHANGE %s %s", timestamp, num, newTemp) != 3)
                {
                    perror("sscanf failed");
                    exit(1);
                }
                int timeStamp = atoi(timestamp);
                int tempSensorIndex = atoi(num);
                float newTemperature = atof(newTemp);
                if (waitTillTimestamp(&startTime, timeStamp) == 0)
                {
                    fprintf(stderr, "waitTillTimestamp - Varibles defined inproperly\n");
                    exit(1);
                }
                else
                {
                    pthread_mutex_lock(&memory->tempsensor[tempSensorIndex].mutex);
                    memory->tempsensor[tempSensorIndex].temperature = newTemperature;
                    pthread_mutex_unlock(&memory->tempsensor[tempSensorIndex].mutex);
                    pthread_cond_signal(&memory->tempsensor[tempSensorIndex].cond);
                }
            }
        }
        else if (strstr(lineA, "SCENARIO"))
        {
            foundScenario = 1;
        }
    }
    ifshutdown = 1;

    fclose(fhA);
}

//<summary>
// Function sleeps till timestamp returns a 0 if time to wait already elapsed
//</summary>
int waitTillTimestamp(struct timeval *startTime, int microseconds_to_wait)
{
    struct timeval current_time;

    /* Start time in microsec */
    long startTime_microsec = startTime->tv_sec * 1000000 + startTime->tv_usec;

    /* Get the current time */
    gettimeofday(&current_time, NULL);

    /* Current time in micro secs */
    long currentTime_microsec = current_time.tv_sec * 1000000 + current_time.tv_usec;

    long timeElapsed_microsec = currentTime_microsec - startTime_microsec;

    if (timeElapsed_microsec < microseconds_to_wait)
    {
        long remaining_microseconds = microseconds_to_wait - timeElapsed_microsec;

        /* Sleep for the remaining time */
        usleep(remaining_microseconds);
        return 1;
    }
    else
    {
        return 1;
    }
}

//<summary>
// Thread function to handle the door scenario
//</summary>
void *handleDoorScenario(void *arg)
{
    struct door_data doorData = *(struct door_data *)arg;
    /* Lock the mutex to protect shared data */
    pthread_mutex_lock(&doorData.door->mutex);
    while (!(ifshutdown))
    {
        /* Wait on cond_start */
        pthread_cond_wait(&doorData.door->cond_start, &doorData.door->mutex);

        /* Upon waking up, check the status */
        if (doorData.door->status == 'O' || doorData.door->status == 'C')
        {
            /* Wait on cond_start again */
            pthread_cond_wait(&doorData.door->cond_start, &doorData.door->mutex);
        }
        else
        {
            /* Sleep for {door open/close time} microseconds */
            usleep(doorData.door_open_duration);

            /* Change the status */
            if (doorData.door->status == 'o')
            {
                doorData.door->status = 'O';
            }
            else if (doorData.door->status == 'c')
            {
                doorData.door->status = 'C';
            }
            /* Signal cond_end to notify others that the operation is complete */
            pthread_cond_signal(&doorData.door->cond_end);
        }
    }
    free(arg);
    return NULL;
}
