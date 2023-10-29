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

/// <summary>
/// Function takes input of address from command line in form of 127.0.0.1:80 and splits
/// into address 127.0.0.1 and port 80.
/// </summary>
int splitAddressPort(char *full_addr, char *addr)
{
    int port_number;
    /* Use strtok to split the input string using ':' as the delimiter */
    char *token = strtok((char *)full_addr, ":");
    if (token != NULL)
    {
        /* token now contains "127.0.0.1" */

        memcpy(addr, token, 9);
        addr[9] = '\0';

        token = strtok(NULL, ":");
        if (token != NULL)
        {
            port_number = atoi(token); /* Store the port as an integer */
            return port_number;
        }
        else
        {
            fprintf(stderr, "Invalid input format of port number.\n");
            exit(1);
        }
    }
    else
    {
        fprintf(stderr, "Invalid input format of port number.\n");
        exit(1);
    }
}

/// <summary>
/// Function that helps establish connection with a program over TCP.
/// </summary>
int tcpConnectTo(int program_port, const char *program_addr)
{
    int fd;

    /*Create TCP IP socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket()");
        exit(1);
    }

    /* Declare a data structure to specify the socket address (IP address + Port)
    memset is used to zero the struct out */

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(program_port);
    if (inet_pton(AF_INET, program_addr, &addr.sin_addr) != 1)
    {
        fprintf(stderr, "inet_pton(%s)\n", program_addr);
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
// Function that helps send message to any program over TCP
//</summary>
unsigned int tcpSendMessageTo(const char *buf, const int program_port, const char *program_addr, int ifClose)
{
    tcpSendMessageTo
        /* Connects to overseer before sending message */
        int fd = tcp_Connect_To(program_port, program_addr);
    if (fd == -1)
    {
        fprintf(stderr, "connect_to");
        return -1;
    }

    /* Sends the message in the buffer*/
    if (send(fd, buf, strlen(buf), 0) == -1)
    {
        fprintf(stderr, "send()");
        return -1;
    }

    if (ifClose)
    {
        /* Close connection */
        if (close(fd) == -1)
        {
            fprintf(stderr, "close()");
            return -1;
        }
        return 1;
    }
    else // Usually for listening for a response
    {
        return fd; // returns file discriptor
    }
    return 1;
}

//<summary>
// Closes connection with the fd
//</summary>
void closeConnection(int client_fd)
{

    /* close the socket used to receive data */
    if (close(client_fd) == -1)
    {
        perror("close_connection close()");
        exit(1);
    }
}
//<summary>
// Shut down and closes connection with the fd
//</summary>
void closeShutdownConnection(int client_fd)
{
    /* Shut down socket - ends communication*/
    if (shutdown(client_fd, SHUT_RDWR) == -1)
    {
        perror("shutdown()");
        exit(1);
    }

    /* close the socket used to receive data */
    if (close(client_fd) == -1)
    {
        perror("closeShutdown_connection close()");
        exit(1);
    }
}
//<summary>
// Send messeage to the fd
//</summary>
void sendMessage(int fd, char *message)
{
    if (send(fd, message, strlen(message), 0) == -1)
    {
        perror("send()");
        exit(1);
    }
}

// <summary>
// Function to receive a message from a fd
// </summary>
ssize_t receiveMessage(int socket, char *buffer, size_t buffer_size)
{
    ssize_t bytes = recv(socket, buffer, buffer_size - 1, 0);
    if (bytes == -1)
    {
        perror("recv()");
        exit(1);
    }
    buffer[bytes] = '\0'; // Null-terminate the received message
    return bytes;
}
