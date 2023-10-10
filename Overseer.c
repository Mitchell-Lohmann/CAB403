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



#define BUFFER_SIZE 1024

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


/* Overseer shared memory structure */
typedef struct {
    char security_alarm; // '-' if inactive, 'A' if active
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}shm_overseer;

/* Function Definitions */

void recv_looped(int fd, void *buf, size_t sz)
{
    char *ptr = buf;
    size_t remain = sz;

    while (remain > 0)
    {
        ssize_t received = read(fd, ptr, remain);
        if (received == -1)
        {
            perror("read()");
            exit(1);
        }
        ptr+= received;
        remain -= received;
    }
}


char *receive_msg(int fd)
{
    uint32_t nlen;
    recv_looped(fd, &nlen, sizeof(nlen));
    uint32_t len = ntohl(nlen);

    char *buf = malloc(len + 1);
    buf[len] = '\0';
    recv_looped(fd, buf, len);
    return buf;
}

void *handleCardReader(void * p_client_socket) ;

bool checkValid(const char *cardSearch, const char *cardID);

int main(int argc, char **argv) 
{
    /* Check for error in input arguments */
    // if(argc < 8)
    // {
    //     fprintf(stderr, "Missing command line arguments, {address:port} {door open duration (in microseconds)} {datagram resend delay (in microseconds)} {authorisation file} {connections file} {layout file} {shared memory path} {shared memory offset}");
    //     exit(1);
    // }

    /* Initialise input arguments */

    char *full_addr = argv[1];
    char addr[10];
    int port ;
    //int door_open_duration = atoi(argv[2]);
    //int datagram_resend_delay = atoi(argv[3]);
    //char *authorisation_file = argv[4];
    //char *connection_file = argv[5];
    //char *layout_file = argv[6];
    //const char *shm_path = argv[7];
    //int shm_offset = atoi(argv[8]);

    
    // Use strtok to split the input string using ':' as the delimiter
    char *token = strtok((char *)full_addr, ":");
    if (token != NULL) {
        // token now contains "127.0.0.1"
          
        memcpy(addr, token, 9);
        addr[9] = '\0';

        token = strtok(NULL, ":");
        if (token != NULL) {
            port = atoi(token); // Store the port as an integer
        } 
        else 
        {
            perror("Invalid input format of port number.\n");
            exit(1);
        }
    } 
    else 
    {
        perror("Invalid input format of address.\n");
        exit(1);

    }



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
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1){
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct*/
    servaddr.sin_family =AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrlen = sizeof(servaddr);

    /* Assign a name to the socket created */
    if (bind(server_socket, (struct sockaddr *)&servaddr, addrlen)==-1) 
    {
        perror("bind()");
        exit(1);
    }

    /*Place server in passive mode - listen for incoming cient request*/
    if (listen(server_socket, 100)==-1) 
    {
        perror("listen()");
        exit(1);
    }

    /* Infinite Loop */
    while (1) 
    {
        /* Generate a new socket for data transfer with the client */
        
        client_socket = accept(server_socket, (struct sockaddr *)&clientaddr, (socklen_t *)&addr_size);
        if (client_socket==-1) 
        {
            perror("accept()");
            exit(1);
        }

        /* Initialise pthreads */
        pthread_t cardreader_thread;
        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;


        if (pthread_create(&cardreader_thread,NULL, handleCardReader, pclient) != 0) 
        {
            perror("pthread_create()");
            exit(1);
        }

        
    } // end while
} // end main


// <summary>
//Function to handle an individual card reader connection
// </summary>
void *handleCardReader(void * p_client_socket) {
    // char* access;
    int client_socket = *((int*)p_client_socket);
    free(p_client_socket); // we dont need it anymore
    char buffer[BUFFER_SIZE];
    char id[4];
    char scanned[17];
    ssize_t bytes;

    bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[bytes] = '\0';
    if (bytes == -1)
    {
        perror("recv()");
        exit(1);
    }
    fflush(stdout);

    /* Validation */
    // Parse the received message (e.g., "CARDREADER {id} HELLO#")
    // Check if initialisation message
    if (strstr(buffer, "HELLO#") != NULL) {

        if (sscanf(buffer, "CARDREADER %s HELLO#", id) == 1)
        {
        printf("Card reader initialised succefully\n");
        printf("Received card reader ID: %s\n", id);
        }
        else
        {
            // Invalid message format
            printf("Invalid message format from card reader.\n");
        }
     
    }
    else if (sscanf(buffer, "CARDREADER %s SCANNED %[^#]#", id, scanned) == 2) 
    {
        // Handle the extracted card reader ID and scanned code
        printf("Received card reader ID: %s\n", id);
        printf("Received scanned code: %s\n", scanned);

        if (checkValid(scanned, id) == true)
        {
            printf("ALLOWED#");
        }
        else
        {
            printf("NOTALLOWED#");
        }
        printf("Over");

    }
    else
    {
        // Invalid message format
        printf("Invalid message format from card reader.\n");
    }
    // Close the connection
    /* Shut down socket - ends communication*/
    if (shutdown(client_socket, SHUT_RDWR) == -1){
        perror("shutdown()");
        exit(1);
    }

	/* close the socket used to receive data */
	if (close(client_socket) == -1)
	{
	    perror("exit()");
		exit(1);
	}   
    return NULL;
}


bool checkValid(const char *cardSearch, const char *cardID) {
    printf("Hey im in the function");
    FILE *fh1 = fopen("authorisation.txt", "r");
    FILE *fh2 = fopen("connections.txt", "r");
    char line[100];  // Assuming a line won't exceed 100 characters
    char *doorID = NULL;

    if (fh1 == NULL) {
        perror("Error opening authorisation file");
        return false;
    }

    if (fh2 == NULL) {
        perror("Error opening connections file");
        return false;
    }
        
    while (fgets(line, sizeof(line), fh2)) {
        char *token = strtok(line, " "); // Split the line by space

        // Check if the first token is DOOR
        if (strcmp(token, "DOOR") == 0) {
            token = strtok(NULL, " "); // Split the line by space
            if (strcmp(token, cardID) == 0)
            {
                token = strtok(NULL, " "); // Split the line by space

                doorID = token;
                /* printf("Door associated with card reader is %s", doorID); */
                break;
            }
        }
    }

    if (doorID == NULL){
        fclose(fh1);
        fclose(fh2);
        /* printf("user with card %s does not have to door through card reader %s \n", cardSearch, doorID); debug */
        return false;
    }

    // Read the file line by line
    while (fgets(line, sizeof(line), fh1)) {
        // Remove newline character if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0'; 
        }

        // Check if the card number is found in the line
        if (strstr(line, cardSearch) != NULL) {
            // If found check if the door is also found in the same line
            if (strstr(line, doorID) != NULL) {
                /* Card number was found in authorisation file with access to desired door. Close file and return true */
                fclose(fh1);
                fclose(fh2);
                return true; 
            }
        }
    }

    /* Card number was not found in authorisation file. Close file and return false */
    fclose(fh1);
    fclose(fh2);
    return false;
}