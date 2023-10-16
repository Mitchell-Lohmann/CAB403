#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>  
#include <fcntl.h>      
#include "common.h"

/* Code is written to be complaint with saftey standards  MISRA-C and IEC 61508. */

/* Call point unit shared memory struct initialisation */
typedef struct {
    char alarm; /* '-' if inactive, 'A' if active */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_firealarm;

int main(int argc, char **argv) {
    /* Check for error in input arguments */
    if(argc < 9) {
        fprintf(stderr, "Missing command line arguments, {address:port} {temperature threshold} {min detections} {detection period (in microseconds)} {reserved argument} {shared memory path} {shared memory offset} {overseer address:port}");
        exit(1);
    }

    /* Initialise input arguments */
    char *full_addr_firealarm = argv[1];
    char firealarm_addr[10];
    int firealarm_port = split_Address_Port(full_addr_firealarm, firealarm_addr);
    int temp_threshold = atoi(argv[2]);
    int min_detections = atoi(argv[3]);
    int detection_period = atoi(argv[4]);
    const char *shm_path = argv[6];
    int shm_offset = atoi(argv[7]);
    char overseer_addr;
    int overseer_port = split_Address_Port(argv[8], overseer_addr);
    char buff[BUFFER_SIZE];

    /* Initialize the UDP socket for receiving messages */
    int recvsockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvsockfd == -1) {
        perror("socket()");
        exit(1);
    }

    /* Enable for re-use of address */
    int opt_enable = 1;
    if (setsockopt(recvsockfd, SOL_SOCKET, SO_REUSEADDR, &opt_enable, sizeof(opt_enable)) == -1) {
        perror("setsockopt()");
        exit(1);
    } 

    /* Initialise the address struct */
    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(firealarm_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the UDP socket*/
    if (bind(recvsockfd, (const struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind()");
        exit(1);
    }

    /* Write msg into buf */
    sprintf(buff, "FIREALARM %s:%d HELLO#\n", firealarm_addr, firealarm_port);

    /* Send initialisation message to overseer */
    if (send_message_to_overseer(buff, overseer_port, overseer_addr) == -1) {
        perror("send_message_to_overseer()");
        exit(1);
    }
    /* Normal function */
} /* End main */