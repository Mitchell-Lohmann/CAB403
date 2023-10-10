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

int split_Address_Port(char *full_addr, char *addr);

int connect_to_overseer(int overseer_port,const char *overseer_addr);
void send_message(const char *buf, const int overseer_port, const char *overseer_addr);




#endif //COMMON_H