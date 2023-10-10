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












    return 0;
}