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

/* Function declaration */
char getDoorStatus(shm_door *shared);

int main(int argc, char **argv)
{
    /* Check for error in input arguments */
    if (argc < 7)
    {
        fprintf(stderr, "Missing command line arguments, {id} {address:port} {FAIL_SAFE | FAIL_SECURE} {shared memory path} {shared memory offset} {overseer address:port} \n");
        exit(1);
    }

    /* Initialise input arguments */
    int id = atoi(argv[1]);
    char door_addr[10];
    char *door_full_addr = argv[2];
    int door_port = splitAddressPort(door_full_addr, door_addr);
    char *initial_config = argv[3];
    const char *shm_path = argv[4];
    int shm_offset = atoi(argv[5]);
    char overseer_addr[10];
    char *overseer_full_addr = argv[6];
    int overseer_port = splitAddressPort(overseer_full_addr, overseer_addr);

    /* Initialisation */
    /* Send buffer, defined in common.h to be consitent across all elements. */
    char buff[BUFFER_SIZE];

    /* Write msg into buf */
    sprintf(buff, "DOOR %d %s:%d %s#\n", id, door_addr, door_port, initial_config);

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
    if (setsockopt(door_socket, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1)
    {
        perror("setsockopt()");
        exit(1);
    }

    /* Initialise the address struct */
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(door_port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrlen = sizeof(servaddr);

    /* Assign a name to the socket created */
    if (bind(door_socket, (struct sockaddr *)&servaddr, addrlen) == -1)
    {
        perror("bind()-Door");
        exit(1);
    }

    /* Place server in passive mode - listen for incoming cient request */
    if (listen(door_socket, 100) == -1)
    {
        perror("listen()");
        exit(1);
    }

    /* Sends the initialisation message to overseer */
    tcpSendMessageTo(buff, overseer_port, overseer_addr, 1);

    /* Open share memory segment */
    int shm_fd = shm_open(shm_path, O_RDWR, 0666); // Creating for testing purposes
    if (shm_fd == -1)
    {
        perror("shm_open()");
        exit(1);
    }

    /* Fstat helps to get information of the shared memory like its size */
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

    /* Shared is used to access the shared memory */
    shm_door *shared = (shm_door *)(shm + shm_offset);

    /* Normal operations for Door */
    char currentDoorStatus = getDoorStatus(shared);
    char *respose;
    int emergency_mode = 0; /* Default: Not in emergency mode */

    for (;;)
    {
        /* Resets the buffer */
        memset(buff, 0, sizeof(buff));

        /* Waits till overseer try to connect to Door */
        client_socket = accept(door_socket, NULL, NULL);
        if (client_socket == -1)
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

        if (strcmp(buff, "OPEN#") == 0)
        {
            if (currentDoorStatus == 'O')
            {
                /* Door already open sends "ALREADY#" to overeer */
                respose = "ALREADY#";

                /* Sends message and closes the connection */
                sendMessage(client_socket, respose);
                closeConnection(client_socket);
            }
            else if (currentDoorStatus == 'C')
            {
                respose = "OPENING#";

                /* Door currently closed sends "OPENING#" to overseer */
                sendMessage(client_socket, respose);

                pthread_mutex_lock(&shared->mutex);
                shared->status = 'o';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                respose = "OPENED#";

                /* Door currently open sends "OPENED#" to overseer */
                sendMessage(client_socket, respose);

                /* Close connection */
                closeConnection(client_socket);

                /* Changes current door status */
                currentDoorStatus = getDoorStatus(shared);
            }
            else
            {
                perror("Current door status NULL");
                exit(1);
            }
        }
        else if ((strcmp(buff, "CLOSE#") == 0) && emergency_mode == 0)
        {
            if (currentDoorStatus == 'C')
            {
                /* Door already open sends "ALREADY#" to overeer */
                respose = "ALREADY#";
                sendMessage(client_socket, respose);

                /* Close connection */
                closeConnection(client_socket);
            }
            else if (currentDoorStatus == 'O')
            {
                respose = "CLOSING#";

                /* Door currently opened sends "CLOSING#" to overseer */
                sendMessage(client_socket, respose);

                pthread_mutex_lock(&shared->mutex);
                shared->status = 'c';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                respose = "CLOSED#";

                /* Door currently open sends "OPENED#" to overseer */
                sendMessage(client_socket, respose);

                /* Close connection */
                closeConnection(client_socket);

                /* Changes current door status */
                currentDoorStatus = getDoorStatus(shared);
            }
            else
            {
                perror("Current door status NULL");
                exit(1);
            }
        }
        else if (strcmp(buff, "OPEN_EMERG#") == 0)
        {
            if (currentDoorStatus == 'C')
            {
                pthread_mutex_lock(&shared->mutex);
                shared->status = 'o';
                pthread_cond_signal(&shared->cond_start);
                pthread_cond_wait(&shared->cond_end, &shared->mutex);
                pthread_mutex_unlock(&shared->mutex);

                /* Set emergency mode */
                emergency_mode = 1;

                /* Close connection */
                closeConnection(client_socket);

                /* Changes current door status */
                currentDoorStatus = getDoorStatus(shared);
            }
            else if (currentDoorStatus == 'O')
            {
                /* Set emergency mode */
                emergency_mode = 1;

                /* Close connection */
                closeConnection(client_socket);

                /* Changes current door status */
                currentDoorStatus = getDoorStatus(shared);
            }
        }
        /* Don't have to implement for group of 2 */
        else if (strcmp(buff, "CLOSE_SECURE#") == 0)
        {
            continue;
            /* Do nothing */
        }
        else if ((strcmp(buff, "CLOSE#") == 0) && emergency_mode == 1)
        {
            /* respond with EMERGENCY_MODE# */
            respose = "EMERGENCY_MODE#";
            sendMessage(client_socket, respose);

            /* Close connection */
            closeConnection(client_socket);
        }
        else
        {
            perror("Received message incorrect format");
            exit(1);
        }
    }

} /* End main */

/* Function definition */
//<summary>
// Gets the status of the door from the shared memory segment.
//</summary>
char getDoorStatus(shm_door *shared)
{
    char door_status;
    /* Lock mutex, store current status of door, unlock mutex */
    pthread_mutex_lock(&shared->mutex);
    door_status = shared->status;
    pthread_mutex_unlock(&shared->mutex);

    return door_status;
}