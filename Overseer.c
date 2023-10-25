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
#include "common.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Overseer shared memory structure */
typedef struct
{
    char security_alarm; // '-' if inactive, 'A' if active
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_overseer;

/* Struct for storing Door data */
struct DoorData
{
    int id;
    struct in_addr door_addr;
    in_port_t door_port;
    char fail_safe; // y if yes (fail_safe) n is no (fail_secure)
    bool acknowledged;
};

/* Global Variables */

struct DoorData DoorList[50]; // Assuming no more than 50 doors will connect
int numDoor = 0;              // Initialise count of door
int doorOpenDuration;         // Door open Duration
int datagramResendDelay;      // Datagram resend delay
char firealarm_addr[10];      // Stores fire alarms addr
int firealarm_port;           // Stores fire alarms port number
pthread_mutex_t doorListMutex;

struct door_reg_datagram
{
    char header[4]; // {'D', 'O', 'O', 'R'}
    struct in_addr door_addr;
    in_port_t door_port;
};

/* Struct for storing thread data */
struct ThreadData
{
    int client_socket;
    pthread_mutex_t mutex;
    char buffer[BUFFER_SIZE]; // Buffer for sending strings between threads
};

/* Function Definitions */

void *handleTCP(void *p_client_socket);

int sendDoorRegDatagram(void *args);

int handleCardScan(int client_socket, char* string);

int checkValid(const char *cardSearch, const char *cardID);

int initializeDoorData(struct DoorData *door, const char *buffer, int ifFailSafe);

int main(int argc, char **argv)
{
    printf("Overseer Launched at address %s\n", argv[1]);
    /* Check for error in input arguments */
    // if(argc < 8)
    // {
    //     fprintf(stderr, "Missing command line arguments, {address:port} {door open duration (in microseconds)} {datagram resend delay (in microseconds)} {authorisation file} {connections file} {layout file} {shared memory path} {shared memory offset}");
    //     exit(1);
    // }

    /* Initialise input arguments */

    char *full_addr = argv[1];
    char addr[10];
    int port = split_Address_Port(full_addr, addr);
    doorOpenDuration = atoi(argv[2]);
    datagramResendDelay = atoi(argv[3]);
    // char *authorisation_file = argv[4];
    // char *connection_file = argv[5];
    // char *layout_file = argv[6];
    // const char *shm_path = argv[7];
    // int shm_offset = atoi(argv[8]);

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

    /* Initialise the address struct*/
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

    /*Place server in passive mode - listen for incoming cient request*/
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
    (void)memset(&addr, 0, sizeof(UDP_addr));
    UDP_addr.sin_family = AF_INET;
    UDP_addr.sin_port = htons(port);
    UDP_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the UDP socket*/
    if (bind(UDP_recvsockfd, (const struct sockaddr *)&UDP_addr, sizeof(UDP_addr)) == -1)
    {
        perror("bind()");
        exit(1);
    }
    
    pthread_mutex_init(&doorListMutex, NULL); // Initialise mutex for doorlist

    // // Malloc memory to store socket fd
    // int *UDP_socket = malloc(sizeof(int));
    // *UDP_socket = UDP_recvsockfd;

    // // Spawn a thread to take care receiving
    // pthread_t UDP;

    // if (pthread_create(&UDP,NULL, NULL, UDP_socket) != 0)
    // {
    //     perror("pthread_create()");
    //     exit(1);
    // }


    /* Infinite Loop */
    while (1)
    {
        /* Generate a new socket for data transfer with the client */

        client_socket = accept(server_socket, (struct sockaddr *)&clientaddr, (socklen_t *)&addr_size);
        if (client_socket == -1)
        {
            perror("accept()");
            exit(1);
        }

        // Create Thread Data on the heap
        struct ThreadData *thread_data = (struct ThreadData *)malloc(sizeof(struct ThreadData));
        if (thread_data == NULL)
        {
            perror("malloc");
            exit(1);
        }

        pthread_mutex_init(&thread_data->mutex, NULL); // Initialise mutex for scanned char
        int *tcp_socket = malloc(sizeof(int));
        *tcp_socket = client_socket;


        /* Initialise pthreads */
        pthread_t thread;

        if (pthread_create(&thread, NULL, handleTCP, tcp_socket) != 0)
        {
            perror("pthread_create()");
            exit(1);
        }


    } // end while

    pthread_mutex_destroy(&doorListMutex);

} // end main

// <summary>
// Function to handle a TCP connection
// </summary>
void *handleTCP(void *p_tcp_socket)
{
    // Access shared socket fd
    int client_socket = *(int *)p_tcp_socket;
    free(p_tcp_socket); // We dont need it anymore

    struct DoorData Door; // store information of a door

    char firealarm_full_addr[16];
    char buffer[BUFFER_SIZE];
    char cardReaderID[4];
    ssize_t bytes;

    // Receive the message send from the card reader
    bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[bytes] = '\0';
    if (bytes == -1)
    {
        perror("recv()");
        exit(1);
    }


    /* Validation */
    // Parse the received message (e.g., "CARDREADER {id} HELLO#")
    // Check if initialisation message except door
    if (strstr(buffer, "HELLO#") != NULL)
    {

        if (sscanf(buffer, "CARDREADER %s HELLO#", cardReaderID) == 1)
        {
            // Do nothing just close connection
            close_connection(client_socket);
            return NULL;
        }
        else if (sscanf(buffer, "FIREALARM %s HELLO#", firealarm_full_addr) == 1)
        {
            // handle Fire alarm initialisation
            printf("Fire alarm innit \n");
            firealarm_port = split_Address_Port(firealarm_full_addr, firealarm_addr);

            // Sends door reg datagram to fire alarm
            sendDoorRegDatagram(NULL);

            close_connection(client_socket);
            return NULL;
        }
        else
        {
            // Invalid message format
            printf("Invalid init message format.\n");
        }
        return NULL;
    }
    // check initialisation for door
    else if (strstr(buffer, "FAIL_SAFE") != NULL)
    {
        // Initialize door
        if (initializeDoorData(&Door, buffer, 1) == 0)
        {
            fprintf(stderr, "initializeDoorData()");
            exit(1);
        }

        pthread_mutex_lock(&doorListMutex);
        // Add door to door list and increment current number of doors
        DoorList[numDoor] = Door;
        numDoor++;
        pthread_mutex_unlock(&doorListMutex);

        // Sends DOOR reg datagram to Fire alarm
        if (sendDoorRegDatagram(NULL) != 0)
        {
            fprintf(stderr, "Did not send Door reg datagram\n");
            exit(1);
        }

        // printf("Num of doors in the list : %d\n", numDoor);

        // Closes the connection
        close_connection(client_socket);
        return NULL;
    }
    else if (strstr(buffer, "FAIL_SECURE") != NULL)
    {
        // initialise Fail secure door
        if (initializeDoorData(&Door, buffer, 0) == 0)
        {
            fprintf(stderr, "initializeDoorData()");
            exit(1);
        }

        pthread_mutex_lock(&doorListMutex);
        // Add door to door list and increment current number of doors
        DoorList[numDoor] = Door;
        numDoor++;
        pthread_mutex_unlock(&doorListMutex);

        printf("Num of doors in the list : %d\n", numDoor);

        // Closes the connection
        close_connection(client_socket);
        return NULL;
    }
    else if (strstr(buffer, "SCANNED") != NULL)
    {
        // Function handles card scan
        if (handleCardScan(client_socket, buffer) != 0)
        {
            fprintf(stderr, "Could no handle card scan\n");
            exit(1);
        }

        return NULL;
    }
    else
    {
        // Invalid message format
        printf("%s", buffer);
        printf("Invalid message.\n");
    }

    fprintf(stderr, "Did not enter if in handleTCP\n");
    return NULL;
}


// <summary>
// Handles card scan
// </summary>
int handleCardScan(int client_socket, char* string)
{

    char scanned[17];
    int doorControllerID;
    char cardReaderID[4];

    char *access;

    if (sscanf(string, "CARDREADER %s SCANNED %[^#]#", cardReaderID, scanned) == 2)
    {
        // printf("Received card reader ID: %s\n", id);
        // printf("Received scanned code: %s\n", scanned);
        doorControllerID = checkValid(scanned, cardReaderID); // returns 0 if not valid
        if (doorControllerID != 0)
        { // Access is allowed
            access = "ALLOWED#";
            send_message(client_socket, access); // Sends the response
            // Closes the connection
            close_connection(client_socket);

            int doorfd; // Store fd of door
            int doorIndex;
            // Send OPEN# to Door Controller
            for (int i = 0; i < numDoor; i++) // Check for the Door ID
            {
                if (DoorList[i].id == doorControllerID)
                {
                    char *msg = "OPEN#";
                    // Sends message to Door Controller setting last param to zero doesnt close connection
                    doorfd = send_message_to(msg, (int)DoorList[i].door_port, inet_ntoa(DoorList[i].door_addr), 0);
                    doorIndex = i;
                    break; // Can break out of loop
                }
                else
                {
                    continue;
                }
            }

            // Receive the message send from the Door
            ssize_t bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
            // printf("Received %s from door",string);

            /* Check if msg received is OPENING */
            if (strcmp(string, "OPENING#") == 0)
            {
                /* Wait till u receive OPENED# */
                bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
                // printf("Received %s from door",string);

                // Check if msg received is open
                if (strcmp(string, "OPENED#") == 0)
                {
                    /* Close connection */
                    close_connection(doorfd);
                    /* Wait for door_open duration */
                    usleep(doorOpenDuration);

                    char *msg = "CLOSE#";
                    /* Open connection again and send CLOSE# (leaves connection open)*/
                    doorfd = send_message_to(msg, (int)DoorList[doorIndex].door_port, inet_ntoa(DoorList[doorIndex].door_addr), 0);
                    // printf("door fd: %d\n", doorfd);

                    /* Extra */
                    // Receive response from Door
                    bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
                    // printf("Received %s from door",string);

                    if (strcmp(string, "CLOSING#") == 0)
                    {
                        // fprintf(stderr, "Got CLOSING#\n");
                        bytes = receiveMessage(doorfd, string, BUFFER_SIZE);
                        if (strcmp(string, "CLOSED#") == 0)
                        {
                            // fprintf(stderr, "Got CLOSED#\n");

                            close_connection(doorfd);
                        }
                    }
                }
                else
                {
                    fprintf(stderr, "Did not receive OPENED# from Door");
                    exit(1);
                }
            }
            else
            {
                fprintf(stderr, "Did not receive OPENING# from door");
                exit(1);
            }
        }
        else
        { // Access is denied
            access = "DENIED#";
            send_message(client_socket, access); // Sends the response
            // Closes the connection
            close_connection(client_socket);
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
        // fire alarm not initialised yet child thread returns
        return 0;
    }
    else if (numDoor == 0)
    {
        // No Doors initialised yet child thread returns
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
    printf("%d\n",numDoor);
    for (int i = 0; i < numDoor; i++)
    {
        // mutex lock
        if (DoorList[i].fail_safe == 'y' && DoorList[i].acknowledged == false) // Fail safe door and Door reg datagram not send
        {
            struct door_reg_datagram regDatagram;
            memcpy(regDatagram.header, "DOOR", 4);         // Writes DOOR into header
            regDatagram.door_addr = DoorList[i].door_addr; // Copies value of door addr
            regDatagram.door_port = DoorList[i].door_port; // Copies value of door port

            // Send door reg datagram to overseer
            (void)sendto(sendfd, &regDatagram, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, sendtoaddr_len);
            
            // Check if dreg received 

            // Update current door acknowledgement status to true
            pthread_mutex_lock(&doorListMutex);
            DoorList[i].acknowledged = true;
            pthread_mutex_unlock(&doorListMutex);

            printf("New acknowledgement %d\n", DoorList[i].acknowledged);
        }
        else
        {
            continue;
        }
    }

    close_connection(sendfd);
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
    char lineA[100]; // Assuming a line won't exceed 100 characters
    char lineB[100]; // Assuming a line won't exceed 100 characters
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
        char *token = strtok(lineA, " "); // Split the line by space

        // Check if the first token is DOOR
        if (strcmp(token, "DOOR") == 0)
        {
            token = strtok(NULL, " "); // Split the line by space
            if (strcmp(token, cardID) == 0)
            {
                token = strtok(NULL, " "); // Split the line by space
                doorID = atoi(token);      // Allocate memory and copy the value
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

    // Read the file line by line
    while (fgets(lineB, sizeof(lineB), fhA))
    {
        char *token = strtok(lineB, " "); // Split the line by space
        // Check if the first token is the user ID we're looking for
        if (strcmp(token, cardSearch) == 0)
        {
            // Read doors to which the user has access, return true if match the door they are at.
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
            break; // No need to continue reading the file
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

    // Check if the buffer matches the DOOR format
    if (sscanf(buffer, "DOOR %s %s FAIL_SAFE#", doorID, door_full_addr) == 2)
    {
        door->id = atoi(doorID);
        door->door_port = split_Address_Port(door_full_addr, door_addr);

        // Use inet_aton to convert the IP string to an in_addr structure
        if (inet_aton(door_addr, &door->door_addr) == 0)
        {
            // Handling the case where the IP string is invalid
            fprintf(stderr, "Error: Invalid IP address\n");
            return 0; // Return an error code to indicate failure
        }

        if (ifFailSafe)
        {
            door->fail_safe = 'y'; // Assuming it's always 'y' for fail_safe
        }
        else if (!(ifFailSafe))
        {
            door->fail_safe = 'n'; // Assuming it's always 'y' for fail_safe
        }

        door->acknowledged = false;

        // printf("Door ID: %d\n", door->id);
        // printf("Door Port: %d\n", door->door_port);
        // printf("Door IP: %s\n", inet_ntoa(door->door_addr));
        // printf("Fail Safe: %c\n", door->fail_safe);

        return 1; // Return 1 to indicate success
    }

    return 0; // Return 0 to indicate failure (input doesn't match the DOOR format)
}


            // for(;;) // Loop to check if overseer receives DREG datagram
            // {
            //     // Wait for datagram resend delay
            //     usleep(datagramResendDelay);

            //     // Receive DREG datagram
            //     ssize_t bytes = recvfrom(receivefd, buff, 512, 0, NULL, NULL);
            //     if (bytes == -1)
            //     {
            //         perror("recvfrom()");
            //         exit(1);
            //     }

            //     if (!(strncmp(buff, "DREG", 4) == 0))
            //     {
            //         // Send door reg datagram to overseer
            //         (void)sendto(sendfd, &regDatagram, (size_t)sizeof(struct door_reg_datagram), 0, (const struct sockaddr *)&sendtoaddr, sendtoaddr_len);

            //     }
            //     else if((strncmp(buff, "DREG", 4) == 0))
            //     {
            //         printf("Received successfully\n");
            //         break;
            //     }
            //     else
            //     {
            //         printf("Looped receive DREG once\n");
            //     }
               
            // }