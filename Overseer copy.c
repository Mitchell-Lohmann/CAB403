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
struct DoorData{
    int id;
    struct in_addr door_addr;
    in_port_t door_port;
    char fail_safe; // y if yes (fail_safe) n is no (fail_secure)
};

/* Struct for storing thread data */
struct ThreadData{
    int client_socket;
    struct DoorData DoorList[50]; // Assuming no more than 50 doors will connect
    int numDoor;
    char firealarm_addr[10];
    int firealarm_port;
    int doorOpenDuration;
};



/* Function Definitions */


void *handleTCP(void *p_client_socket);

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
    int door_open_duration = atoi(argv[2]);
    // int datagram_resend_delay = atoi(argv[3]);
    // char *authorisation_file = argv[4];
    // char *connection_file = argv[5];
    // char *layout_file = argv[6];
    // const char *shm_path = argv[7];
    // int shm_offset = atoi(argv[8]);



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
    int opt_enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1)
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

    // Create Thread Data on the heap
    struct ThreadData* thread_data = (struct ThreadData*)malloc(sizeof(struct ThreadData));
    if (thread_data == NULL) {
        perror("malloc");
        exit(1);
    }

    thread_data->numDoor = 0;// initialise current num of doors to 0
    thread_data->doorOpenDuration = door_open_duration;

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

        // Store client socket to thread_data
        thread_data->client_socket = client_socket;


        /* Initialise pthreads */
        pthread_t cardreader_thread;


        if (pthread_create(&cardreader_thread, NULL, handleTCP, thread_data) != 0)
        {
            perror("pthread_create()");
            exit(1);
        }

        pthread_join(cardreader_thread, NULL);



    } // end while

    free(thread_data);


} // end main

// <summary>
// Function to handle an individual card reader connection
// </summary>
void *handleTCP(void *p_thread_data)
{
    // Access shared data
    struct ThreadData *thread_data = (struct ThreadData *)p_thread_data;
    int client_socket = thread_data->client_socket;

    struct DoorData Door; // store information of a door

    int doorControllerID;
    char *access;
    char firealarm_full_addr[16];
    char buffer[BUFFER_SIZE];
    char cardReaderID[4];
    char scanned[17];
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
            // Close and shutdown the connection
            //printf("Received card reader ID: %s\n", cardReaderID);
            close_connection(client_socket);
            return NULL;
            
        }
        else if (sscanf(buffer, "FIREALARM %s HELLO#", firealarm_full_addr) == 1)
        {
            // handle Fire alarm initialisation
            printf("Fire alarm innit\n");
            thread_data->firealarm_port = split_Address_Port(firealarm_full_addr, thread_data->firealarm_addr);
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
        if (initializeDoorData(&Door, buffer, 1) == 0) {
            fprintf(stderr, "initializeDoorData()");
            exit(1);
        }

        // Add door to door list
        thread_data->DoorList[thread_data->numDoor] = Door;
        thread_data->numDoor++;

        // printf("Num of doors in the list : %d\n", thread_data->numDoor);


        // Closes the connection
        close_connection(client_socket);
        return NULL;
    }
    else if (strstr(buffer, "FAIL_SECURE") != NULL)
    {
        // initialise Fail secure door
        if (initializeDoorData(&Door, buffer, 0) == 0) {
            fprintf(stderr, "initializeDoorData()");
            exit(1);
        }
        // Add door to door list
        thread_data->DoorList[thread_data->numDoor] = Door;
        thread_data->numDoor++;

        // printf("Num of doors in the list : %d\n", thread_data->numDoor);
    
        // Closes the connection
        close_connection(client_socket);
        return NULL;
    }
    else if (sscanf(buffer, "CARDREADER %s SCANNED %[^#]#", cardReaderID, scanned) == 2)
    {
        // Handle the extracted card reader ID and scanned code
        // printf("Received card reader ID: %s\n", id);
        // printf("Received scanned code: %s\n", scanned);

        doorControllerID = checkValid(scanned, cardReaderID); // returns 0 if not valid
        if (doorControllerID != 0)
        {
            access = "ALLOWED#";
        }
        else
        {
            access = "DENIED#";
        }
        /* Sends the message back to card_reader*/
        if (send(client_socket, access, strlen(access), 0) == -1)
        {
            perror("send()");
            exit(1);
        }
        // Closes the connection
        close_connection(client_socket);


    }
    else
    {
        // Invalid message format
        printf("%s", buffer);
        printf("Invalid message format from card reader.\n");
    }

    // Door open
    if (strcmp(access , "ALLOWED#") == 0)
    {
        int doorfd; // Store fd of door
        int doorIndex;
        // Send OPEN# to Door Controller
        for (int i = 0; i < thread_data->numDoor;i++) // Check for the Door ID
        {
            printf("Im in the loop \n");
            if (thread_data->DoorList[i].id == doorControllerID)
            {
                printf("Im in the if\n");
                char *buffer = "OPEN#";
                // Sends message to Door Controller setting last param to zero doesnt close connection
                doorfd = send_message_to(buffer, thread_data->DoorList[i].door_port, inet_ntoa(thread_data->DoorList[i].door_addr) , 0);
                printf("Msg send\n");
                doorIndex = i;
                break; // Can break out of loop
            }
        }

        // Receive the message send from the Door 
        bytes = receiveMessage(doorfd, buffer, BUFFER_SIZE);
        
        if (strcmp(buffer , "OPENING#") == 0)
        {
            // Wait till u receive OPENED#

            bytes = receiveMessage(doorfd, buffer, BUFFER_SIZE);

            if (strcmp(buffer, "OPENED#") == 0)
            {
                // Close connection
                close_connection(doorfd);
                usleep(thread_data->doorOpenDuration); // Wait for door_open duration
                char *buffer = "CLOSE#";
                // Sends message to Door and closes connection
                doorfd = send_message_to(buffer, thread_data->DoorList[doorIndex].door_port, inet_ntoa(thread_data->DoorList[doorIndex].door_addr) , 0);
                printf("door fd: %d\n", doorfd);

                bytes = receiveMessage(doorfd, buffer, BUFFER_SIZE);
                if (strcmp(buffer , "CLOSING#") == 0) {
                    fprintf(stderr, "Got CLOSING#\n");
                    bytes = receiveMessage(doorfd, buffer, BUFFER_SIZE);
                    if (strcmp(buffer , "CLOSED#") == 0) {
                        fprintf(stderr, "Got CLOSED#\n");

                        close_connection(doorfd);
                    }
                }
            }
            else
            {
                printf("%s\n", buffer);
                fprintf(stderr, "Invalid response from Door\n");
                exit(1);
            }
            
        }
        else
        {
            printf("%s\n", buffer);
            fprintf(stderr, "Invalid response from Door\n");
            exit(1);
        }


    }
    else
    {
        fprintf(stderr, "Access not defined yet\n");
        exit(1);
    }


    // Close and shutdown the connection
    //close_connection(client_socket);
    return NULL;
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
//Takes care of initialising a new door controller
//</summary>
int initializeDoorData(struct DoorData *door, const char *buffer, int ifFailSafe) {
    char door_full_addr[16];
    char door_addr[10];
    char doorID[4];

    // Check if the buffer matches the DOOR format
    if (sscanf(buffer, "DOOR %s %s FAIL_SAFE#", doorID, door_full_addr) == 2) {
        door->id = atoi(doorID);
        door->door_port = split_Address_Port(door_full_addr, door_addr);

        // Use inet_aton to convert the IP string to an in_addr structure
        if (inet_aton(door_addr, &door->door_addr) == 0) {
            // Handling the case where the IP string is invalid
            fprintf(stderr, "Error: Invalid IP address\n");
            return 0; // Return an error code to indicate failure
        }

        if (ifFailSafe)
        {
            door->fail_safe = 'y'; // Assuming it's always 'y' for fail_safe
        }else if (!(ifFailSafe))
        {
            door->fail_safe = 'n'; // Assuming it's always 'y' for fail_safe
        }

        // printf("Door ID: %d\n", door->id);
        // printf("Door Port: %d\n", door->door_port);
        // printf("Door IP: %s\n", inet_ntoa(door->door_addr));
        // printf("Fail Safe: %c\n", door->fail_safe);

        return 1; // Return 1 to indicate success
    }

    return 0; // Return 0 to indicate failure (input doesn't match the DOOR format)
}

//<summary>
//Send Door registration datagram to firealarm
//</summary>
int handleUDP(int firealarmPort, char* firealarmAddr)
{
    /* Create send socket to send UDP datagram to fire alarm */
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd == -1) {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address of firealarm (address + Port) */
    struct sockaddr_in sendtoaddr;
    (void)memset(&sendtoaddr, 0, sizeof(sendtoaddr));
    sendtoaddr.sin_family = AF_INET;
    sendtoaddr.sin_port = htons(firealarmPort);
    if (inet_pton(AF_INET, firealarmAddr, &sendtoaddr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", firealarmAddr);
        exit(1);
    }
    //socklen_t senderaddr_len = sizeof(sendtoaddr);

    // Create Dreg Datagram
    return 0;


}