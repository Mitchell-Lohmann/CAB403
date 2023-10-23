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

int split_Address_Port(char *full_addr, char *addr);
int connect_to(int overseer_port,const char *overseer_addr);
int send_message_to(const char *buf, const int overseer_port, const char *overseer_addr, int ifClose);
void closeShutdown_connection(int client_fd);
void close_connection(int client_fd);
void send_message(int fd, char *message);
ssize_t receiveMessage(int socket, char* buffer, size_t buffer_size);



#endif //COMMON_H