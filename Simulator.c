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

/* Global variables */
int cardReaderNum = 0; // Initialise variables to keep count of initialised programs
int doorNum = 0;       // Initialise variables to keep count of initialised programs
int tempsensorNum = 0; // Initialise variables to keep count of initialised programs
int callpointNum = 0;  // Initialise variables to keep count of initialised programs
int portNumber = 3000; // Initial Port Number
// Initialises shared memory
char *shm_path = "/shm";

int overseerPort;
int fireAlarmPort;
int doorPort[20];
int temPort[20];

char overseerAddress[256];

pthread_mutex_t globalMutex;

/// <summary>
/// Function takes input of scenario file name and runs all initialisation setup
/// commands. Returns no value. Errors if scenario file is not valid.
/// </summary>
void init(char *scenarioName)
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
    int overseerCount = 1; 
    //int doorCount = 1;
    //int cardreaderCount = 1;
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

                    ssize_t offset = overseerCount * sizeof(shm_overseer); // Calculates the offset for overseer

                    char shm_offset[256]; // char buffer for storing the offset

                    snprintf(shm_offset, sizeof(shm_offset), "%zd", offset); // Convert offset to string

                    char door_open_duration[64], datagram_resend_delay[64], authorisation_file[64], connection_file[64], layout_file[64]; // To grab input argument from text file

                    /* Check that sscanf is successful */
                    if (sscanf(lineA, "INIT overseer %s %s %s %s %s", door_open_duration, datagram_resend_delay, authorisation_file, connection_file, layout_file) != 5)
                    {
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(overseerAddress, 256, "%s:%d", serverAddress, overseerPort);

                    execl("overseer", "./overseer", overseerAddress, door_open_duration, datagram_resend_delay, authorisation_file, connection_file, layout_file, shm_path, shm_offset, NULL);

                    perror("execl");
                }

            }
            if (child_pid == 0) // Child process
            {
                return;
                // if (!strcmp(token, "door"))
                // {
                //     // printf("door found in line %s\n", lineA);
                //     /* Check that sscanf is successful */
                //     if (sscanf(lineA, "INIT %s %s %s %s", argument1, argument2, argument3, argument4) != 4)
                //     {
                //         perror("sscanf failed");
                //         exit(1);
                //     }

                //     snprintf(argumentAddressPort, 128, "%s:%d", serverAddress, serverPort);

                //     strcpy(argument0, "./door");

                //     execl(argument0, argument1, argument2, argumentAddressPort, argument3, "/shm", argument4, "127.0.0.1:3000", NULL);
                //     perror("execl");

                //     //
                // }
                // else if (!strcmp(token, "cardreader"))
                // {
                //     // printf("cardreader found in line %s\n", lineA);
                // }
                // else if (!strcmp(token, "firealarm"))
                // {
                //     // printf("firealarm found in line %s\n", lineA);
                // }
                // else if (!strcmp(token, "callpoint"))
                // {
                //     // printf("callpoint found in line %s\n", lineA);
                // }
                // else if (!strcmp(token, "tempsensor"))
                // {
                //     // printf("tempsensor found in line %s\n", lineA);
                // }
                // else if (!strcmp(token, "destselect"))
                // {
                //     // printf("destselect found in line %s\n", lineA);
                // }
                // else if (!strcmp(token, "camera"))
                // {
                //     // printf("camera found in line %s\n", lineA);
                // }
            }
        }
        else
        {
            break; // All innit lines over
        }
    }
}

void initOverseer(char *scenarioName, char serverAddress[16], int serverPort)
{
    /* Create child process */
    pid_t child_pid = fork();

    /* Parent process to become overseer */
    if (child_pid > 0)
    {
        FILE *fhA = fopen(scenarioName, "r");
        char lineA[100]; // Assuming a line won't exceed 100 characters

        /* Check to see if file opened */
        if (fhA == NULL)
        {
            perror("Error opening scenario file");
            return;
        }

        fgets(lineA, sizeof(lineA), fhA);

        char argumentAddressPort[64], argument0[64], argument1[64], argument2[64], argument3[64], argument4[64], argument5[64], argument6[64];

        /* Check that sscanf is successful */
        if (sscanf(lineA, "INIT overseer %s %s %s %s %s", argument2, argument3, argument4, argument5, argument6) != 5)
        {
            perror("sscanf failed");
            exit(1);
        }

        snprintf(argumentAddressPort, 128, "%s:%d", serverAddress, serverPort);

        execl("./overseer", "overseer", argumentAddressPort, argument2, argument3, argument4, argument5, argument6, NULL);
        perror("execl");
    }

    /* Child process to continue as simulator */
    if (child_pid == 0)
    {
        return;
        printf("Child launched at ID %d", child_pid);
    }

    /* Confirm successful creation of child process */
    if (child_pid == -1)
    {
        perror("Fork failed");
        exit(1);
    }
}

/* Function Definitions */
void initSharedMemory(char *shm_path, sharedMemory *memory);

void initSharedStructs(char *scenarioName, sharedMemory *memory);

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

    sharedMemory memory;

    // Remove any previous instance of the shared memory object, if it exists.
    shm_unlink(shm_path);

    // This function initialises the shared memory
    initSharedMemory(shm_path, &memory);

    // This functions innitialises the mutex and condvars and the default values in the struct
    initSharedStructs(scenarioName, &memory);

    init(scenarioName);

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

    memory = mmap(NULL, sizeof(sharedMemory), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, shm_fd, 0);
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
                pthread_cond_init(&memory->overseer.cond, &cattr);  // Initialise the condvar in shared memory

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

                memset(memory->cardreader[cardReaderNum].scanned, '\0', sizeof(memory->cardreader[cardReaderNum].scanned)); // Set scanned array to '\0'
                memory->cardreader->response = '\0';                                                                        // set response char to '\0'

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
    // Can destroy the condition attribute as it's no longer needed
    pthread_condattr_destroy(&cattr);
    pthread_mutexattr_destroy(&mattr);
}