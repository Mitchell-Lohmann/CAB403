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


// 1023 so can add null term if req
#define BUFFER_SIZE 1023

/* Card reader shared-memory struct initialisation */
typedef struct
{
    char scanned[16];
    pthread_mutex_t mutex;
    pthread_cond_t scanned_cond;

    char response; // 'Y' or 'N' (or '\0' at first)
    pthread_cond_t response_cond;
} shm_cardreader;

/*Function Definitions*/

void send_message(const char *buf, const int overseer_port, const char *overseer_addr);

int connect_to_overseer(int overseer_port, const char *overseer_addr);



/* Start Main */

int main(int argc, char **argv)
{
     /* Check for error in input arguments */
    if(argc < 6)
    {
        fprintf(stderr, "Missing command line arguments, {id} {wait time (in microseconds)} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    
    /* Initialise input arguments */
    int id = atoi(argv[1]);
    int waittime = atoi(argv[2]);
    const char *shm_path = argv[3];
    int shm_offset = atoi(argv[4]);
    char *full_addr = argv[5];

    char overseer_addr[10];
    int overseer_port ;

    // Use strtok to split the input string using ':' as the delimiter
    char *token = strtok((char *)full_addr, ":");
    if (token != NULL) {
        // token now contains "127.0.0.1"
          
        memcpy(overseer_addr, token, 9);
        overseer_addr[9] = '\0';

        token = strtok(NULL, ":");
        if (token != NULL) {
            overseer_port = atoi(token); // Store the port as an integer
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

    /* Initialisation */

    /* Send buffer */
    char buff[BUFFER_SIZE];

    /* Write msg into buf */
    sprintf(buff, "CARDREADER {%d} HELLO#\n", id);
    
    /* Send message to overseer */
    send_message(buff, overseer_port, overseer_addr);
    
    // printf("Send this msg to overseer %s \n", buff);



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

    char *shm = mmap(NULL, shm_stat.st_size,PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm == MAP_FAILED)
    {
        perror("mmap()");
        exit(1);
    }

    /* shared is used to access the shared memory */
    shm_cardreader *shared = (shm_cardreader *)(shm + shm_offset);

    /* Normal Operation for cardreader */

    pthread_mutex_lock(&shared->mutex);

    for(;;)
    {
        if(shared->scanned[0] != '\0')
        {
            char buf[17];
            memcpy(buf, shared->scanned, 16);
            buf[16] = '\0';

            sprintf(buff, "CARDREADER %d SCANNED %s#", id , buf);

            /* Connects to overseer and sends the message in the buffer*/
            send_message(buff, overseer_port, overseer_addr);

            /* Need to implement overseer here, has been skipped in example video. */
            shared->response = 'Y';
            pthread_cond_signal (&shared->response_cond);
        }
        pthread_cond_wait(&shared->scanned_cond, &shared->mutex);
    }
    close(shm_fd);

} // end main



//<summary> 
//Function that helps establish connection with overseer 
//</summary>
int connect_to_overseer(int overseer_port,const char *overseer_addr)
{
    int fd;

    /*Create TCP IP socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /*Declare a data structure to specify the socket address (IP address + Port)
     *memset is used to zero the struct out
     */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(overseer_port);
    const char *ipaddress = overseer_addr;
    if (inet_pton(AF_INET, overseer_addr, &addr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", overseer_addr);
        exit(1);
    }

    /* Connect to overseer */
    if (connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("connect()");
        exit(1);
    }

    return fd;
}


//<summary> 
//Function that helps send message to overseer 
//</summary>
void send_message(const char *buf, const int overseer_port, const char *overseer_addr)
{
    /* Connects to overseer before sending message */
    int fd = connect_to_overseer(overseer_port, overseer_addr);
 
    /* Sends the message in the buffer*/
    if (send(fd, buf, strlen(buf), 0) == -1)
    {
        perror("send()");
        exit(1);
    }

    /* Close connection */
    if (close(fd) == -1)
    {
        perror("close()");
        exit(1);
    }
}
