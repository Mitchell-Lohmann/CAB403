#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

// common.h
// Include guards to prevent multiple inclusions
#ifndef COMMON_H
#define COMMON_H

#define BUFFER_SIZE 1024

// Struct definitions 
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



int split_Address_Port(char *full_addr, char *addr);
int connect_to(int overseer_port,const char *overseer_addr);
int send_message_to(const char *buf, const int overseer_port, const char *overseer_addr, int ifClose);
void closeShutdown_connection(int client_fd);
void close_connection(int client_fd);
void send_message(int fd, char *message);
ssize_t receiveMessage(int socket, char* buffer, size_t buffer_size);



#endif //COMMON_H