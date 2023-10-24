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
#include <stdbool.h>

typedef struct
{
    char status; /* '-' for inactive, '*' for active */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_callpoint;

typedef struct
{
    char scanned[16];
    pthread_mutex_t mutex;
    pthread_cond_t scanned_cond;

    char response; // 'Y' or 'N' (or '\0' at first)
    pthread_cond_t response_cond;
} shm_cardreader;

typedef struct
{
    char status; /* 'O' for open, 'C' for closed, 'o' for opening, 'c' for closing */
    pthread_mutex_t mutex;
    pthread_cond_t cond_start;
    pthread_cond_t cond_end;
} shm_door;

typedef struct
{
    char alarm; /* '-' if inactive, 'A' if active */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_firealarm;

typedef struct
{
    char security_alarm; // '-' if inactive, 'A' if active
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} shm_overseer;


typedef struct {
    float temperature;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}shm_tempsensor;


typedef struct{
    shm_overseer overseer;
    shm_firealarm firealarm;
    shm_cardreader cardreader[40];
    shm_door doors[20];
    shm_tempsensor tempsensor[20];
    shm_callpoint callpoint[20];

}sharedMemory;