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
#include <sys/time.h>
#include <fcntl.h>
#include "common.h"


typedef struct {
    float temperature;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}shm_tempsensor;

int main(int argc, char **argv)
{
    /* Check for error in input arguments */
    if (argc < 8)
    {
        fprintf(stderr, "Missing command line arguments, {id} {address:port} {max condvar wait (microseconds)} {max update wait (microseconds)} {shared memory path} {shared memory offset} {receiver address:port}...");
        exit(1);
    }

    int id = atoi(argv[1]);
    char *full_addr = argv[2];
    char tempsensor_addr[10];
    int tempsensor_port = split_Address_Port(full_addr, tempsensor_addr);
    int max_condvar_wait = atoi(argv[3]);
    int max_update_wait = atoi(argv[4]);
    char *shm_path = argv[5];
    int shm_offset = atoi(argv[6]);
    
    int receiverNum = argc - 7 ; // Total number of receiving tempsensors
    char receiver_addr[10]; // All receivers have same addr

    int receiver_port[receiverNum]; // Array storing receiver_port

    for(int i = 0; i < receiverNum; i++)
    {
        char *receiver_full_addr = argv[7 + i];
        receiver_port[i] = split_Address_Port(receiver_full_addr, receiver_addr);

    }

    








    return 0;

}