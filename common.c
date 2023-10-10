#include <stdio.h>
#include "common.h"


//<summary>
//splits the address into address and port
//</summary>
int split_Address_Port(char *full_addr, char *addr)
{
    int port_number;
// Use strtok to split the input string using ':' as the delimiter
    char *token = strtok((char *)full_addr, ":");
    if (token != NULL) {
        // token now contains "127.0.0.1"
          
        memcpy(addr, token, 9);
        addr[9] = '\0';

        token = strtok(NULL, ":");
        if (token != NULL) {
            port_number = atoi(token); // Store the port as an integer
            return port_number;
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
}

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