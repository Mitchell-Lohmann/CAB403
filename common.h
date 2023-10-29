#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

/* common.h */
/* Include guards to prevent multiple inclusions */
#ifndef COMMON_H
#define COMMON_H

#define BUFFER_SIZE 1024

/* Struct definitions */
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

    char response; /* 'Y' or 'N' (or '\0' at first) */
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
    char security_alarm; /* '-' if inactive, 'A' if active */
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

/* Datagram to be send from callpoint and overseer to firealarm in case of fire*/
typedef struct{
    char header[4]; /* {'F', 'I', 'R', 'E'} */
} callpoint_datagram;

/* Datagram format */
struct addr_entry {
  struct in_addr sensor_addr;
  in_port_t sensor_port;
};

/* Datagram send from tempsensors to other tempsensors, overseer*/
struct datagram_format {
  char header[4]; // {'T', 'E', 'M', 'P'}
  struct timeval timestamp;
  float temperature;
  uint16_t id;
  uint8_t address_count;
  struct addr_entry address_list[50];
};

struct door_reg_datagram{
    char header[4]; // {'D', 'O', 'O', 'R'}
    struct in_addr door_addr;
    in_port_t door_port;
};

struct door_confirm_datagram{
    char header[4]; // {'D', 'R', 'E', 'G'}
    struct in_addr door_addr;
    in_port_t door_port;
};

int splitAddressPort(char *full_addr, char *addr);
int tcpConnectTo(int overseer_port,const char *overseer_addr);
unsigned int tcpSendMessageTo(const char *buf, const int overseer_port, const char *overseer_addr, int ifClose);
void closeShutdownConnection(int client_fd);
void closeConnection(int client_fd);
void sendMessage(int fd, char *message);
ssize_t receiveMessage(int socket, char* buffer, size_t buffer_size);

#endif //COMMON_H