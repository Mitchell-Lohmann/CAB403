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

/* Code is written to be complaint with saftey standards  MISRA-C and IEC 61508. */

/* Door controller shared memory struct initialisation */
typedef struct {
    char status; /* 'O' for open, 'C' for closed, 'o' for opening, 'c' for closing */
    pthread_mutex_t mutex;
    pthread_cond_t cond_start;
    pthread_cond_t cond_end;
}shm_door;

/* Function Definition */
//<summary>
// Gets the status of the door from the shared memory segment.
//</summary>
char get_door_status (shm_door* shared)
{
    char door_status;
    /* Lock mutex, store current status of door, unlock mutex */
    pthread_mutex_lock(&shared->mutex);
    door_status = shared->status;
    pthread_mutex_unlock(&shared->mutex);

    return door_status;
}

//<summary>
// Chnages door status, takes input of shared memory location and status to change 
// to. Returns no value. 
//</summary>
void change_door_status (shm_door* shared, char status_to_changeto)
{
    pthread_mutex_lock(&shared->mutex);
    shared->status = status_to_changeto;
    pthread_mutex_unlock(&shared->mutex);
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
    char door_addr[10];
    int door_port = split_Address_Port(argv[2], door_addr);

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

    /* Declare a data structure to specify the socket address (IP address + Port)
    memset is used to zero the struct out */
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    /* Create TCP IP Socket */
    door_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (door_socket == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Enable for re-use address */
    int opt_enable = 1;
    if (setsockopt(door_socket, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1){
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct */
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

    /* Place server in passive mode - listen for incoming cient request */
    if (listen(door_socket, 100)==-1) 
    {
        perror("listen()");
        exit(1);
    }

    /* Sends the initialisation message to overseer */
    send_message_to_overseer(buff, overseer_port, overseer_addr);

    /* Open share memory segment */
    int shm_fd = shm_open(shm_path, O_RDWR, 0666); // Creating for testing purposes
    if(shm_fd == -1){
        perror("shm_open()");
        exit(1);
    }
    
    /* Fstat helps to get information of the shared memory like its size */
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

    /* Shared is used to access the shared memory */
    shm_door *shared = (shm_door *)(shm + shm_offset);

    /* Normal operations for Door */
    char current_door_status = get_door_status(shared);
    char *respose;
    int emergency_mode = 0; /* Default: Not in emergency mode */


    for(;;) 
    {
        /* Resets the buffer */
        memset(buff, 0, sizeof(buff));

        /* Waits till overseer try to connect to Door */
        client_socket = accept(door_socket, NULL, NULL);
        if (client_socket==-1) 
        {
            perror("accept()");
            exit(1);
        }

        /* Stores the message received from overseer to a buffer */
        ssize_t bytes = recv(client_socket, buff, BUFFER_SIZE, 0);
        buff[bytes] = '\0';
        if (bytes == -1)
        {
            perror("recv()");
            exit(1);
        }

        if (strcmp(buff, "OPEN#") == 0 )
        {
            if(current_door_status == 'O')
            {
                /* Door already open sends "ALREADY#" to overeer */
                respose = "ALREADY#";

                /* Sends message and closes the connection */
                send_message(client_socket, respose);
                close_connection(client_socket);    
            }
            else if (current_door_status == 'C')
            {
                respose = "OPENING#";
                
                /* Door currently closed sends "OPENING#" to overseer */
                send_message(client_socket, respose);

                pthread_mutex_lock(&shared->mutex);
                shared->status = 'o';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                respose = "OPENED#";

                /* Door currently open sends "OPENED#" to overseer */
                send_message(client_socket, respose);

                /* Close connection */
                close_connection(client_socket);

                /* Changes current door status */
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
                /* Door already open sends "ALREADY#" to overeer */
                respose = "ALREADY#";
                send_message(client_socket, respose);

                /* Close connection */
                close_connection(client_socket);
            }
            else if (current_door_status == 'O')
            {
                respose = "CLOSING#";

                /* Door currently opened sends "CLOSING#" to overseer */
                send_message(client_socket, respose);

                pthread_mutex_lock(&shared->mutex);
                shared->status = 'c';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                respose = "CLOSED#";

                /* Door currently open sends "OPENED#" to overseer */
                send_message(client_socket, respose);


                /* Close connection */
                close_connection(client_socket);

                /* Changes current door status */
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

                /* Set emergency mode */
                emergency_mode = 1;

                /* Close connection */
                close_connection(client_socket);

                /* Changes current door status */
                current_door_status = get_door_status(shared);
            }
            else if (current_door_status == 'O')
            {
                /* Set emergency mode */
                emergency_mode = 1;

                /* Close connection */
                close_connection(client_socket);

                /* Changes current door status */
                current_door_status = get_door_status(shared);
            }
        }
        else if (strcmp(buff, "CLOSE_SECURE#") == 0) // Dont have to implement for group of 2
        {
            continue;
            /* Do nothing */
        }
        else if ((strcmp(buff, "CLOSE#") == 0) && emergency_mode == 1)
        {
            /* respond with EMERGENCY_MODE# */
            respose = "EMERGENCY_MODE#";
            send_message(client_socket, respose);

            /* Close connection */
            close_connection(client_socket);
        }
        else
        {
            perror("Received message incorrect format");
            exit(1);
        }
    } /* End main */

    printf("Program finishes\n"); /* Debug */
    // return 0;
}