#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include "common.h"

/* Max no: of TCP connection the server can handle */
#define MAX_THREADS 100

/* Flag set when exit command entered in overseer or simulator kills process with SIG_TERM */
volatile sig_atomic_t ifShutDown = 0;

struct TempData
{
    struct timeval timestamp;
    float temperature;
    uint16_t id;
};

/* Struct for storing Door data */
struct DoorData
{
    int id;
    struct in_addr door_addr;
    in_port_t door_port;
    /* y if yes (fail_safe) n is no (fail_secure) */
    char fail_safe; 
    bool acknowledged;
};

/* Global Variables */
/* Assuming no more than 50 doors will connect */
struct DoorData DoorList[50];
/* Assuming no more than 50 temp sensors are present */
struct TempData TempList[50];
/* Initialise count of door */
int numDoor = 0;
/* Initialise number of temp sensors */
int numTemp = 0;
/* Door open Duration */
int doorOpenDuration;
/* Datagram resend delay */
int datagramResendDelay;
/* Stores fire alarms addr */
char firealarm_addr[10];
/* Stores fire alarms port number */
int firealarm_port;
/* Flag to keep track if Dreg is received */
int ifDregReceived = 0;
/* To keep track of thread count */
int threadCount = 0;

pthread_t thread_array[MAX_THREADS];
shm_overseer *shared;
pthread_mutex_t globalMutex;
pthread_mutex_t tempMutex;

/* Function declarations */
void *handleTCP(void *p_client_socket);
void *handleUDP(void *p_recvsockfd);
void *handleManualAccess(void *arg);
void *sentCallpointDatagram(void *);
int sendDoorRegDatagram(void *args);
int handleCardScan(int client_socket, char *string);
int checkValid(const char *cardSearch, const char *cardID);
int initializeDoorData(struct DoorData *door, const char *buffer, int ifFailSafe);
int DoorOpen(int doorID);
void DoorClose(int doorID);
void handleTEMPDatagram(struct datagram_format *receivedDatagram);

int main(int argc, char **argv)
{
    
    /* Check for error in input arguments */
    if (argc < 8)
    {
        fprintf(stderr, "Missing command line arguments, {address:port} {door open duration (in microseconds)} {datagram resend delay (in microseconds)} {authorisation file} {connections file} {layout file} {shared memory path} {shared memory offset}");
        exit(1);
    }

    /* Initialise input arguments */

    char *full_addr = argv[1];
    char addr[10];
    int port = splitAddressPort(full_addr, addr);
    doorOpenDuration = atoi(argv[2]);
    datagramResendDelay = atoi(argv[3]);
    const char *shm_path = argv[7];
    int shm_offset = atoi(argv[8]);

    /* Open share memory segment */
    int shm_fd = shm_open(shm_path, O_RDWR, 0666);

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
    shared = (shm_overseer *)(shm + shm_offset);
    /* TCP Initialise */

    /* Client file descriptor */
    int client_socket, server_socket;

    /*Declare a data structure to specify the socket address (IP address + Port)
     *memset is used to zero the struct out */
    struct sockaddr_in servaddr, clientaddr, addr_size;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&servaddr, 0, sizeof(clientaddr));

    /*Create TCP IP Socket*/
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* enable for re-use address*/
    int opt_enable_TCP = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_enable_TCP, sizeof(opt_enable_TCP)) == -1)
    {
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct */
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrlen = sizeof(servaddr);

    /* Assign a name to the socket created */
    if (bind(server_socket, (struct sockaddr *)&servaddr, addrlen) == -1)
    {
        perror("bind()");
        exit(1);
    }

    /* Place server in passive mode - listen for incoming cient request */
    if (listen(server_socket, 100) == -1)
    {
        perror("listen()");
        exit(1);
    }

    /* UDP Initialise */
    /* Initialize the UDP socket for receiving messages */
    int UDP_recvsockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (UDP_recvsockfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Enable for re-use of address */
    int opt_enable_UDP = 1;
    if (setsockopt(UDP_recvsockfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable_UDP, sizeof(opt_enable_UDP)) == -1)
    {
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct for overseer */
    struct sockaddr_in UDP_addr;
    (void)memset(&UDP_addr, 0, sizeof(UDP_addr));
    UDP_addr.sin_family = AF_INET;
    UDP_addr.sin_port = htons(port);
    UDP_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the UDP socket*/
    if (bind(UDP_recvsockfd, (const struct sockaddr *)&UDP_addr, sizeof(UDP_addr)) == -1)
    {
        perror("bind()");
        exit(1);
    }
    /* Initialise mutex for doorlist */
    pthread_mutex_init(&globalMutex, NULL);
    pthread_mutex_init(&tempMutex, NULL);

    /* Malloc memory to store socket fd */
    int *UDP_socket = malloc(sizeof(int));
    *UDP_socket = UDP_recvsockfd;

    /* Spawn a thread to take care receiving UDP datagram and Manual access */
    pthread_t UDP, manualAccess;

    if (pthread_create(&UDP, NULL, handleUDP, UDP_socket) != 0)
    {
        perror("pthread_create()");
        exit(1);
    }

    if (pthread_create(&manualAccess, NULL, handleManualAccess, NULL) != 0)
    {
        perror("pthread_create()");
        exit(1);
    }

    /* Infinite Loop */
    while (!ifShutDown)
    {
        if (threadCount < MAX_THREADS)
        {
            /* Generate a new socket for data transfer with the client */
            client_socket = accept(server_socket, (struct sockaddr *)&clientaddr, (socklen_t *)&addr_size);
            if (client_socket == -1)
            {
                if (errno == EINTR)
                {
                    /* Retry if the call was interrupted by a signal */
                    continue;
                }
                else
                {
                    perror("accept()");
                    exit(1);
                }
            }

            int *tcp_socket = malloc(sizeof(int));
            *tcp_socket = client_socket;

            /* Initialise pthreads */
            pthread_t TCP_thread;

            if (pthread_create(&TCP_thread, NULL, handleTCP, tcp_socket) != 0)
            {
                perror("pthread_create()");
                exit(1);
            }

            /* Add the thread to the thread array */
            thread_array[threadCount++] = TCP_thread;
        }

    }

    if (ifShutDown)
    {
        closeConnection(server_socket);
        pthread_mutex_destroy(&globalMutex);
        pthread_mutex_destroy(&tempMutex);
        pthread_join(UDP, NULL);
        pthread_join(manualAccess, NULL);

        /* wait for all TCP threads to join */
        for (int i = 0; i < threadCount; i++)
        {
            pthread_join(thread_array[i], NULL);
        }
        return 0;
    }

    return 0;

} /* End main */

/* Function definitions */

// <summary>
// Function to handle a TCP connection
// </summary>
void *handleTCP(void *p_tcp_socket)
{
    // Access shared socket fd
    int client_socket = *(int *)p_tcp_socket;
    free(p_tcp_socket);

    /* store information of a door */
    struct DoorData Door;

    char firealarm_full_addr[16];
    char buffer[BUFFER_SIZE];
    char cardReaderID[4];
    ssize_t bytes;

    /* Receive the message send from the card reader */
    bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[bytes] = '\0';
    if (bytes == -1)
    {
        perror("recv()");
        exit(1);
    }

    /* Validation */
    /* Parse the received message (e.g., "CARDREADER {id} HELLO#") */
    /* Check if initialisation message except door */
    if (strstr(buffer, "HELLO#") != NULL)
    {

        if (sscanf(buffer, "CARDREADER %s HELLO#", cardReaderID) == 1)
        {
            /* Do nothing just close connection */
            closeConnection(client_socket);
            return NULL;
        }
        else if (sscanf(buffer, "FIREALARM %s HELLO#", firealarm_full_addr) == 1)
        {
            /* handle Fire alarm initialisation */
            firealarm_port = splitAddressPort(firealarm_full_addr, firealarm_addr);

            /* Sends door reg datagram to fire alarm */
            sendDoorRegDatagram(NULL);

            closeConnection(client_socket);
            return NULL;
        }
        else
        {
            /* Invalid message format */
            printf("Invalid init message format.\n");
        }
        return NULL;
    }
    /* check initialisation for door */
    else if (strstr(buffer, "FAIL_SAFE") != NULL)
    {
        /* Initialize door */
        if (initializeDoorData(&Door, buffer, 1) == 0)
        {
            fprintf(stderr, "initializeDoorData()");
            exit(1);
        }

        pthread_mutex_lock(&globalMutex);
        /* Add door to door list and increment current number of doors */
        DoorList[numDoor] = Door;
        numDoor++;
        ifDregReceived = 0;
        pthread_mutex_unlock(&globalMutex);

        /* Sends DOOR reg datagram to Fire alarm */
        if (sendDoorRegDatagram(NULL) != 0)
        {
            fprintf(stderr, "Did not send Door reg datagram\n");
            exit(1);
        }

        /* Closes the connection */
        closeConnection(client_socket);
        return NULL;
    }
    else if (strstr(buffer, "FAIL_SECURE") != NULL)
    {
        /* initialise Fail secure door */
        if (initializeDoorData(&Door, buffer, 0) == 0)
        {
            fprintf(stderr, "initializeDoorData()");
            exit(1);
        }

        pthread_mutex_lock(&globalMutex);
        /* Add door to door list and increment current number of doors */
        DoorList[numDoor] = Door;
        numDoor++;
        pthread_mutex_unlock(&globalMutex);

        /* Closes the connection */
        closeConnection(client_socket);
        return NULL;
    }
    else if (strstr(buffer, "SCANNED") != NULL)
    {
        /* Function handles card scan */
        if (handleCardScan(client_socket, buffer) != 0)
        {
            fprintf(stderr, "Could no handle card scan\n");
            exit(1);
        }

        return NULL;
    }
    else
    {
        /* Invalid message format */
        printf("%s", buffer);
        printf("Invalid message.\n");
    }

    fprintf(stderr, "Did not enter if in handleTCP\n");
    close(client_socket);
    return NULL;
}

//<summary>
// Thread function to handle UDP datagrams received by overseer
//</summary>
void *handleUDP(void *p_recvsockfd)
{Safe buffer size for UDP communications
/* Safe buffer size for UDP communications */
    char buff[512]; 

    int receivefd = *(int *)p_recvsockfd;
    free(p_recvsockfd);
    struct sockaddr_in clientaddr;
    socklen_t client_size = sizeof(clientaddr);

    for (;;)
    {
        ssize_t bytes = recvfrom(receivefd, buff, 512, MSG_DONTWAIT, (struct sockaddr *)&clientaddr, &client_size);
        if (bytes == -1)
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
            if ((strncmp(buff, "DREG", 4) == 0))
            {

                pthread_mutex_lock(&globalMutex);
                /* Set dreg received flag to 1 */
                ifDregReceived = 1;
                pthread_mutex_unlock(&globalMutex);


                continue;
            }
            else if (strncmp(buff, "TEMP", 4) == 0)
            {
                /* Handle receiving temp datagram */
                struct datagram_format *receivedDatagram = (struct datagram_format *)buff;
                if (receivedDatagram == NULL)
                {
                    fprintf(stderr, "Pointer points to NULL");
                    exit(1);
                }
                /* Update TempList */
                handleTEMPDatagram(receivedDatagram);
            }
            else
            {
                pthread_mutex_lock(&globalMutex);
                /* Set dreg received flag to 0 */
                ifDregReceived = 0; 
                pthread_mutex_unlock(&globalMutex);
                printf("Incorrect format datagram received \n");
            }
        }
        if (ifShutDown)
        {
            /* Shut down flag is set */
            return NULL;
        }
    }
    return NULL;
}

// <summary>
// Function to handle user input
// </summary>
void *handleManualAccess(void *arg)
{
    char input[256];
    pthread_t fire;

    while (1)
    {
        int doorID;
        int ifFireAlarm;

        /* Checks flag */
        if (ifFireAlarm)
        {
            /* Create a thread to continuosly send FIRE datagram to fire alarm */
            /* Resets the flag */
            ifFireAlarm = 0;

            if (pthread_create(&fire, NULL, sentCallpointDatagram, NULL) != 0)
            {
                perror("pthread_create()");
                exit(1);
            }
        }

        printf("Enter a command: \n");
        fgets(input, sizeof(input), stdin);

        /* Parse the command and perform actions based on user input */
        if (strstr(input, "DOOR LIST") != NULL)
        {
            if (numDoor == 0)
            {
                printf("No initialised doors\n");
                continue;
            }
            /* Handle DOOR LIST command */
            /* Print the list of doors */
            for (int i = 0; i < numDoor; i++)
            {
                if (DoorList[i].fail_safe == 'y')
                {
                    printf("DoorID:%d DoorAddr:%s DoorPort:%d DoorConfig:FAIL_SAFE\n", DoorList[i].id, inet_ntoa(DoorList[i].door_addr), ntohs(DoorList[i].door_port));
                }
                else
                {
                    printf("DoorID:%d DoorAddr:%s DoorPort:%d DoorConfig:FAIL_SECURE\n", DoorList[i].id, inet_ntoa(DoorList[i].door_addr), ntohs(DoorList[i].door_port));
                }
            }
        }
        else if (strstr(input, "DOOR OPEN") != NULL)
        {
            if (sscanf(input, "DOOR OPEN %d", &doorID) == 1)
            {
                /* Handle DOOR OPEN command */
                DoorOpen(doorID);
            }
        }
        else if (strstr(input, "DOOR CLOSE") != NULL)
        {
            if (sscanf(input, "DOOR CLOSE %d", &doorID) == 1)
            {
                /* Handle DOOR OPEN command */
                DoorClose(doorID);
            }
        }
        else if (strstr(input, "TEMPSENSOR LIST") != NULL)
        {

            /* Handle TEMPSENSOR LIST command */
            /* Print List of Temp Sensor */
            for (int i = 0; i < numTemp; i++)
            {
                printf("TempsensorID : %d Tempreading: %f\n", TempList[i].id, TempList[i].temperature);
            }
        }
        else if (strstr(input, "FIRE ALARM") != NULL)
        {
            /* Handle FIRE ALARM command */
            /* sets flag if fire alarm */
            ifFireAlarm = 1;
        }
        else if (strstr(input, "SECURITY ALARM") != NULL)
        {
            /* Handle SECURITY ALARM command */
            /* Security alarm has been triggered */

            pthread_mutex_lock(&shared->mutex);
            shared->security_alarm = 'A';
            pthread_mutex_unlock(&shared->mutex);
            pthread_cond_signal(&shared->cond);
            /* Rest not for groups of 2 */
        }
        else if (strstr(input, "EXIT") != NULL)
        {
            /* Handle EXIT command */
            ifShutDown = 1;
        }
        else
        {
            printf("Invalid command\n");
        }

        if (ifShutDown)
        {
            /* Wait for thread fire to join */
            pthread_join(fire, NULL);
            /* Shut down flag is set */
            return NULL;
        }
    }
    return NULL;
}

//<summary>
// Thread function that sends the FIRE datagram to fire alarm waits for resend delay and sends it again
//</summary>
void *sentCallpointDatagram(void *)
{
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
    destaddr.sin_port = htons((in_port_t)firealarm_port);
    if (inet_pton(AF_INET, firealarm_addr, &destaddr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", firealarm_addr);
        exit(1);
    }

    /* Keeps sending datagram */
    for (;;)
    {
        /* Sends UDP Datagram */
        (void)sendto(sendfd, datagram.header, (size_t)strlen(datagram.header), 0, (const struct sockaddr *)&destaddr, destaddr_len);
        /* Sleep for {resend delay} */
        (void)usleep(datagramResendDelay);

        if (ifShutDown)
        {
            /* Shut down flag is set */
            return NULL;
        }

    }

    return NULL;
}

// <summary>
// Handles card scan
// </summary>
int handleCardScan(int client_socket, char *string)
{

    char scanned[17];
    int doorControllerID;
    char cardReaderID[4];

    char *access;

    if (sscanf(string, "CARDREADER %s SCANNED %[^#]#", cardReaderID, scanned) == 2)
    {
        doorControllerID = checkValid(scanned, cardReaderID); // returns 0 if not valid
        if (doorControllerID != 0)
        {
            /* Access is allowed */
            access = "ALLOWED#";
            sendMessage(client_socket, access); // Sends the response
            /* Closes the connection */
            closeConnection(client_socket);

            /* Send OPEN# to Door Controller */
            /* Sends Open, Waits for OPENING, OPENED or ALREADY and close the connection */
            if (DoorOpen(doorControllerID) == 1)
            {
                /* Wait for door_open duration */
                usleep(doorOpenDuration);
                DoorClose(doorControllerID);
            }
        }
        else
        {
            /* Access is denied */
            access = "DENIED#";
            /* Sends the response */
            sendMessage(client_socket, access);
            /* Closes the connection */
            closeConnection(client_socket);
            return 0;
        }
    }
    else
    {
        fprintf(stderr, "Invalid cardscan\n");
        exit(1);
    }

    return 0;
}

// <summary>
// Sends the door registration datagram of doors that are not registered by fire alarm
// </summary>
int sendDoorRegDatagram(void *args)
{
    if (firealarm_port == 0)
    {
        /* Fire alarm not initialised yet child thread returns */
        return 0;
    }
    else if (numDoor == 0)
    {
        /* No Doors initialised yet child thread returns */
        return 0;
    }

    /* Create send socket to send UDP datagram to fire alarm */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address of firealarm (address + Port) */
    struct sockaddr_in sendtoaddr;
    (void)memset(&sendtoaddr, 0, sizeof(sendtoaddr));
    sendtoaddr.sin_family = AF_INET;
    sendtoaddr.sin_port = htons(firealarm_port);
    if (inet_pton(AF_INET, firealarm_addr, &sendtoaddr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", firealarm_addr);
        exit(1);
    }
    socklen_t sendtoaddr_len = sizeof(sendtoaddr);
    for (int i = 0; i < numDoor; i++)
    {
        /* Mutex lock */
        /* Fail safe door and Door reg datagram not send */
        if (DoorList[i].fail_safe == 'y' && DoorList[i].acknowledged == false)
        {
            struct door_reg_datagram regDatagram;
            /* Writes DOOR into header */
            memcpy(regDatagram.header, "DOOR", 4);
            /* Copies value of door addr */
            regDatagram.door_addr = DoorList[i].door_addr;
            /* Copies value of door port */
            regDatagram.door_port = DoorList[i].door_port;

            /* Send door reg datagram to firealarm */
            (void)sendto(sendfd, &regDatagram, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, sendtoaddr_len);

            for (;;)
            {
                usleep(datagramResendDelay);
                /* Check if dreg received */
                if (!(ifDregReceived))
                {
                    /* Send door reg datagram to firealarm */
                    (void)sendto(sendfd, &regDatagram, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, sendtoaddr_len);
                }
                else
                {
                    break;
                }
            }

            /* Update current door acknowledgement status to true */
            pthread_mutex_lock(&globalMutex);
            DoorList[i].acknowledged = true;
            pthread_mutex_unlock(&globalMutex);
        }
        else
        {
            continue;
        }
    }

    closeConnection(sendfd);
    return 0;
}

/// <summary>
/// Function takes input of card number and reader where card was scanned. Checks
/// authorisation.txt and connections.txt to confirm if card is valid. Returns
/// true if card is valid for door or false if card is invalid for door.
/// </summary>
int checkValid(const char *cardSearch, const char *cardID)
{

    FILE *fhA = fopen("authorisation.txt", "r");
    FILE *fhB = fopen("connections.txt", "r");
    /* Assuming a line won't exceed 100 characters */
    char lineA[100];
    char lineB[100];
    int doorID = -1;

    if (fhA == NULL)
    {
        perror("Error opening authorisation file");
        return false;
    }

    if (fhB == NULL)
    {
        perror("Error opening connections file");
        return false;
    }

    while (fgets(lineA, sizeof(lineA), fhB))
    {
        char *token = strtok(lineA, " ");

        /* Check if the first token is DOOR */
        if (strcmp(token, "DOOR") == 0)
        {
            token = strtok(NULL, " ");
            if (strcmp(token, cardID) == 0)
            {
                token = strtok(NULL, " "); 
                doorID = atoi(token);
                break;
            }
        }
    }

    /* If card reader does not match any door in the connections files. */
    if (doorID == -1)
    {
        printf("Could not match card reader ID with door ID \n"); // Debug line
        fclose(fhA);
        fclose(fhB);
        return false;
    }

    /* Read the file line by line */
    while (fgets(lineB, sizeof(lineB), fhA))
    {
        char *token = strtok(lineB, " ");    

        /* Check if the first token is the user ID we're looking for */
        if (strcmp(token, cardSearch) == 0)
        {
            /* Read doors to which the user has access, return true if match the door they are at. */
            while ((token = strtok(NULL, " ")))
            {
                if (strncmp(token, "DOOR:", 5) == 0)
                {
                    if (doorID == atoi(token + 5))
                    {
                        fclose(fhA);
                        fclose(fhB);
                        return doorID;
                    }
                }
            }
            /* No need to continue reading the file */
            break;
        }
    }

    /* Card number was not found in authorisation file. Close file and return false */
    fclose(fhA);
    fclose(fhB);
    return false;
}

//<summary>
// Takes care of initialising a new door controller
//</summary>
int initializeDoorData(struct DoorData *door, const char *buffer, int ifFailSafe)
{
    char door_full_addr[16];
    char door_addr[10];
    char doorID[4];

    /* Check if the buffer matches the DOOR format */
    if (sscanf(buffer, "DOOR %s %s FAIL_SAFE#", doorID, door_full_addr) == 2)
    {
        door->id = atoi(doorID);
        door->door_port = htons(splitAddressPort(door_full_addr, door_addr));

        /* Use inet_aton to convert the IP string to an in_addr structure */
        if (inet_aton(door_addr, &door->door_addr) == 0)
        {
            /* Handling the case where the IP string is invalid */
            fprintf(stderr, "Error: Invalid IP address\n");Return an error code to indicate failure
            /* Return an error code to indicate failure */
            return 0;
        }

        if (ifFailSafe)
        {
            /* Assuming it's always 'y' for fail_safe */
            door->fail_safe = 'y';
        }
        else if (!(ifFailSafe))
        {
            /* Assuming it's always 'y' for fail_safe */
            door->fail_safe = 'n';
        }

        door->acknowledged = false;

        /* Return 1 to indicate success */
        return 1;
    }

    /* Return 0 to indicate failure (input doesn't match the DOOR format) */
    return 0;
}

//<summary>
// Opens door with that id
//</summary>
int DoorOpen(int doorID)
{
    int doorfd;
    char string[BUFFER_SIZE];

    /* Send OPEN# to Door Controller */
    for (int i = 0; i < numDoor; i++) // Check for the Door ID
    {
        if (DoorList[i].id == doorID)
        {
            char *msg = "OPEN#";
            /* Sends message to Door Controller setting last param to zero doesnt close connection 
            */
            doorfd = tcpSendMessageTo(msg, ntohs(DoorList[i].door_port), inet_ntoa(DoorList[i].door_addr), 0);Can break out of loop
            /* Can break out of loop */
            break;
        }
        else
        {
            continue;
        }
    }

    /* Receive the message send from the Door */
    ssize_t bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
    string[bytes] = '\0';
    if (bytes == -1)
    {
        perror("recv()");
        exit(1);
    }

    /* Check if msg received is OPENING */
    if (strcmp(string, "OPENING#") == 0)
    {
        /* Wait till u receive OPENED# */
        bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
        string[bytes] = '\0';
        if (bytes == -1)
        {
            perror("recv()");
            exit(1);
        }

        /* Check if msg received is open */
        if (strcmp(string, "OPENED#") == 0)
        {
            /* Close connection */
            closeConnection(doorfd);
            return 1;
        }
    }
    else if (strcmp(string, "ALREADY#") == 0)
    {
        closeConnection(doorfd);
        return 1;
    }
    else
    {
        fprintf(stderr, "Did not receive valid string in DoorOpen()\n");
        exit(1);
    }
    return 0;
}

void DoorClose(int doorID)
{
    int doorfd;
    char string[BUFFER_SIZE];

    /* Send CLOSE# to Door Controller */
    /* Check for the Door ID */
    for (int i = 0; i < numDoor; i++)
    {
        if (DoorList[i].id == doorID)
        {
            char *msg = "CLOSE#";
            /* Sends message to Door Controller setting last param to zero doesnt close connection */
            doorfd = tcpSendMessageTo(msg, ntohs(DoorList[i].door_port), inet_ntoa(DoorList[i].door_addr), 0);
            /* Can break out of loop */
            break;
        }
        else
        {
            continue;
        }
    }

    /* Receive the message send from the Door */
    ssize_t bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
    string[bytes] = '\0';
    if (bytes == -1)
    {
        perror("recv()");
        exit(1);
    }

    /* Check if msg received is OPENING */
    if (strcmp(string, "CLOSING#") == 0)
    {
        /* Wait till u receive OPENED# */
        bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
        string[bytes] = '\0';
        if (bytes == -1)
        {
            perror("recv()");
            exit(1);
        }

        /* Check if msg received is open */
        if (strcmp(string, "CLOSED#") == 0)
        {
            
            closeConnection(doorfd);
        }
    }
    else if (strcmp(string, "EMERGENCY_MODE#") == 0 || strcmp(string, "ALREADY#") == 0)
    {
        /* Close connection */
        closeConnection(doorfd);
    }
    else
    {
        fprintf(stderr, "Did not receive valid string in DoorClose()\n");
        exit(1);
    }
}

//<summary>
// Updates Temp list when a Temp datagram is received
//</summary>
void handleTEMPDatagram(struct datagram_format *receivedDatagram)
{
    struct timeval current_time;
    /* Get the current time */
    gettimeofday(&current_time, NULL);

    int ifSensorInList = 0;
    int index = -1;

    for (int i = 0; i < numTemp; i++)
    {
        if (receivedDatagram->id == TempList[i].id)
        {

            ifSensorInList = 1;

            int time_elapsed_received = (current_time.tv_sec - receivedDatagram->timestamp.tv_sec) * 1000000 +
                                        (current_time.tv_usec - receivedDatagram->timestamp.tv_usec);

            int time_elapsed_TempList = (current_time.tv_sec - TempList[i].timestamp.tv_sec) * 1000000 +
                                        (current_time.tv_usec - TempList[i].timestamp.tv_usec);

            if (time_elapsed_received < time_elapsed_TempList)
            {
                index = i;
                break;
            }
        }
    }

    if (ifSensorInList)
    {
        if (index != -1)
        {

            pthread_mutex_lock(&tempMutex);
            TempList[index].timestamp = receivedDatagram->timestamp;
            TempList[index].temperature = receivedDatagram->temperature;
            pthread_mutex_unlock(&tempMutex);
        }
    }
    else
    {
        /* Not in the list, add to list */

        /* Create tempSensor */
        struct TempData tempSensor;
        tempSensor.id = receivedDatagram->id;
        tempSensor.temperature = receivedDatagram->temperature;
        tempSensor.timestamp = receivedDatagram->timestamp;

        /* Add to the list */
        pthread_mutex_lock(&tempMutex);
        TempList[numTemp] = tempSensor;
        (numTemp)++;
        pthread_mutex_unlock(&tempMutex);
    }
}
