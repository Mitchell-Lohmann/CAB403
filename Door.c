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


#define BUFFER_SIZE 1023

/* Door controller shared memory struct initialisation */
typedef struct {
    char status; // 'O' for open, 'C' for closed, 'o' for opening, 'c' for closing
    pthread_mutex_t mutex;
    pthread_cond_t cond_start;
    pthread_cond_t cond_end;
}shm_door;


/* Function Definition */

//<summary>
//Gets the status of the door from the shared memory segment
//</summary>
char get_door_status (shm_door* shared)
{
    char door_status;
    // Lock mutex, store current status of door, unlock mutex
    pthread_mutex_lock(&shared->mutex);
    door_status = shared->status;
    pthread_mutex_unlock(&shared->mutex);

    return door_status;
}

void change_door_status (shm_door* shared, char status_to_changeto)
{
    pthread_mutex_lock(&shared->mutex);
    shared->status = status_to_changeto;
    pthread_mutex_unlock(&shared->mutex);
}

//<summary>
//Closes connection with the fd
//</summary>
void close_connection(int client_fd)
{
    /* Shut down socket - ends communication*/
    if (shutdown(client_fd, SHUT_RDWR) == -1){
        perror("shutdown()");
        exit(1);
    }

	/* close the socket used to receive data */
	if (close(client_fd) == -1)
	{
	    perror("exit()");
		exit(1);
	}   
}

int main(int argc, char **argv)
{
     /* Check for error in input arguments */
    if(argc < 7)
    {
        fprintf(stderr, "Missing command line arguments, {id} {address:port} {FAIL_SAFE | FAIL_SECURE} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    /* Initialise input arguments */
    int id = atoi(argv[1]);
    char *full_addr_door = argv[2];
    char door_addr[10];
    int door_port = split_Address_Port(full_addr_door,door_addr);


    char *initial_config = argv[3];
    const char *shm_path = argv[4];
    int shm_offset = atoi(argv[5]);
    char *full_addr_overseer = argv[6];

    char overseer_addr[10];
    int overseer_port = split_Address_Port(full_addr_overseer,overseer_addr);

    /* Initialisation */


    /* Send buffer */
    char buff[BUFFER_SIZE];

    /* Write msg into buf */
    sprintf(buff, "DOOR %d %s:%d %s#\n", id,door_addr,door_port,initial_config);

    /* Client file descriptor */
    int client_socket, door_socket;

    /*Declare a data structure to specify the socket address (IP address + Port)
    *memset is used to zero the struct out */
    struct sockaddr_in servaddr, clientaddr, addr_size;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&servaddr, 0, sizeof(clientaddr));


    /*Create TCP IP Socket*/
    door_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (door_socket == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* enable for re-use address*/
    int opt_enable = 1;
    if (setsockopt(door_socket, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1){
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct*/
    servaddr.sin_family =AF_INET;
    servaddr.sin_port = htons(door_port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrlen = sizeof(servaddr);

    /* Assign a name to the socket created */
    if (bind(door_socket, (struct sockaddr *)&servaddr, addrlen)==-1) 
    {
        perror("bind()");
        exit(1);
    }

    /*Place server in passive mode - listen for incoming cient request*/
    if (listen(door_socket, 100)==-1) 
    {
        perror("listen()");
        exit(1);
    }

    client_socket = accept(door_socket, (struct sockaddr *)&clientaddr, (socklen_t *)&addr_size);
    if (client_socket==-1) 
    {
        perror("accept()");
        exit(1);
    }
    else
    {
        /* Sends the initialisation message to overseer */
        if (send(client_socket, buff, strlen(buff), 0) == -1)
        {
            perror("send()");
            exit(1);
        }

    }

    // Close the connection after sending initialising message

    close_connection(client_socket);


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

    char *shm = mmap(NULL, shm_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    /* shared is used to access the shared memory */
    shm_door *shared = (shm_door *)(shm + shm_offset);

    
    /* Normal operations for Door */

    
    char current_door_status = get_door_status(shared);
    char *respose;
    int emergency_mode = 0; // Default: Not in emergency mode


    for(;;)
    { // Loop starts

        // Resets the buffer 
        memset(buff, 0, sizeof(buff));

        // Waits till overseer try to connect to Door
        client_socket = accept(door_socket, (struct sockaddr *)&clientaddr, (socklen_t *)&addr_size);
        if (client_socket==-1) 
        {
            perror("accept()");
            exit(1);
        }

        // Stores the message received from overseer to a buffer
        ssize_t bytes = recv(client_socket, buff, BUFFER_SIZE, 0);
        buff[bytes] = '\0';
        if (bytes == -1)
        {
            perror("recv()");
            exit(1);
        }
        fflush(stdout);

        if (strcmp(buff, "OPEN#") == 0 )
        {
            if(current_door_status == 'O')
            {
                // Door already open sends "ALREADY#" to overeer
                respose = "ALREADY#";
                if (send(client_socket, respose, strlen(respose), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }
                // Close connection
                close_connection(client_socket);

            }
            else if (current_door_status == 'C')
            {
                respose = "OPENING#";
                
                // Door currently closed sends "OPENING#" to overseer
                if (send(client_socket, respose, strlen(respose), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }

                pthread_mutex_lock(&shared->mutex);
                shared->status = 'o';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                respose = "OPENED#";

                // Door currently open sends "OPENED#" to overseer
                if (send(client_socket, respose, strlen(respose), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }

                // Close connection
                close_connection(client_socket);

                // Changes current door status
                current_door_status = get_door_status(shared);

            }
            else
            {
                perror("Current door status NULL");
                exit(1);
            }

        }
        else if((strcmp(buff, "CLOSE#") == 0) && emergency_mode == 0)
        {
            if(current_door_status == 'C')
            {
                // Door already open sends "ALREADY#" to overeer
                respose = "ALREADY#";
                if (send(client_socket, respose, strlen(respose), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }
                // Close connection
                close_connection(client_socket);

            }
            else if (current_door_status == 'O')
            {
                respose = "CLOSING#";

                // Door currently opened sends "CLOSING#" to overseer
                if (send(client_socket, respose, strlen(respose), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }

                pthread_mutex_lock(&shared->mutex);
                shared->status = 'c';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                respose = "CLOSED#";

                // Door currently open sends "OPENED#" to overseer
                if (send(client_socket, respose, strlen(respose), 0) == -1)
                {
                    perror("send()");
                    exit(1);
                }

                // Close connection
                close_connection(client_socket);

                // Changes current door status
                current_door_status = get_door_status(shared);
                

            }
            else
            {
                perror("Current door status NULL");
                exit(1);
            }

        }
        else if (strcmp(buff, "OPEN_EMERG#") == 0)
        {
            if (current_door_status == 'C')
            {
                pthread_mutex_lock(&shared->mutex);
                shared->status = 'o';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                // Set emergency mode
                emergency_mode = 1;

                // Close connection
                close_connection(client_socket);

                // Changes current door status
                current_door_status = get_door_status(shared);

            }
            else if (current_door_status == 'O')
            {
                // Set emergency mode
                emergency_mode = 1;

                // Close connection
                close_connection(client_socket);

                // Changes current door status
                current_door_status = get_door_status(shared);
            }
            

        }
        else if (strcmp(buff, "CLOSE_SECURE#") == 0) // Dont have to implement for group of 2
        {
            // Do nothing
        }
        else if ((strcmp(buff, "CLOSE#") == 0) && emergency_mode == 1)
        {
            // respond with EMERGENCY_MODE#
        }
        else
        {

        }

        
    } // Loop ends











    printf("Program finishes\n");
    //return 0;
}