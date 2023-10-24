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
#include "commonSimulator.h"
#include "common.h"

/// <summary>
/// Function takes input of scenario file name and runs all initialisation setup
/// commands. Returns no value. Errors if scenario file is not valid.
/// </summary>
void init(char *scenarioName)
{
    int serverPort = 3001;
    char serverAddress[64] = "127.0.0.1";

    /* Read in file and confirm success */
    FILE *fhA = fopen(scenarioName, "r");
    char lineA[100]; // Assuming a line won't exceed 100 characters
    char lineCompare[100];
    // int currentPort = 3000; // Starting port number

    /* Check to see if file opened */
    if (fhA == NULL)
    {
        perror("Error opening scenario file");
        return;
    }

    /* Check each line for "INIT, if found find the process and execl() it"*/
    while (fgets(lineA, sizeof(lineA), fhA) != NULL)
    {
        strcpy(lineCompare, lineA);
        if (strstr(lineA, "INIT"))
        {
            char *token = strtok(lineCompare, " "); // Split the line by space
            token = strtok(NULL, " ");

            /* Create child process */
            pid_t child_pid = fork();

            /* Confirm successful creation of chirl process */
            if (child_pid == -1)
            {
                perror("Fork failed");
                return;
            }

            char argumentAddressPort[64], argument0[64], argument1[64], argument2[64], argument3[64], argument4[64], argument5[64], argument6[64];

            if (child_pid == 0)
            {

                if (!strcmp(token, "overseer"))
                {
                    /* Check that sscanf is successful */
                    if(sscanf(lineA, "INIT %s %s %s %s %s %s", argument1, argument2, argument3, argument4, argument5, argument6) != 6){
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(argumentAddressPort, 128, "%s:%d", serverAddress, 3000);

                    strcpy(argument0, "./overseer");
                    execl(argument0, argument1, argumentAddressPort, argument2, argument3, argument4, argument5, argument6, NULL);
                    perror("execl");

                }
                else if (!strcmp(token, "door"))
                {
                    //printf("door found in line %s\n", lineA);
                     /* Check that sscanf is successful */
                    if(sscanf(lineA, "INIT %s %s %s %s", argument1, argument2, argument3, argument4) != 4){
                        perror("sscanf failed");
                        exit(1);
                    }

                    snprintf(argumentAddressPort, 128, "%s:%d", serverAddress, serverPort);

                    strcpy(argument0, "./door");

                    execl(argument0, argument1, argument2, argumentAddressPort, argument3, "/shm", argument4, "127.0.0.1:3000", NULL);
                    perror("execl");
                }
                else if (!strcmp(token, "cardreader"))
                {
                    //printf("cardreader found in line %s\n", lineA);
                }
                else if (!strcmp(token, "firealarm"))
                {
                    //printf("firealarm found in line %s\n", lineA);
                }
                else if (!strcmp(token, "callpoint"))
                {
                    //printf("callpoint found in line %s\n", lineA);
                }
                else if (!strcmp(token, "tempsensor"))
                {
                    //printf("tempsensor found in line %s\n", lineA);
                }
                else if (!strcmp(token, "destselect"))
                {
                    //printf("destselect found in line %s\n", lineA);
                }
                else if (!strcmp(token, "camera"))
                {
                    //printf("camera found in line %s\n", lineA);
                }
            }
            serverPort++;
        }
    }
}

void initOverseer(char *scenarioName, char serverAddress[16], int serverPort){
    /* Create child process */
    pid_t child_pid = fork();

    /* Parent process to become overseer */
    if(child_pid != 0){
        FILE *fhA = fopen(scenarioName, "r");
        char lineA[100]; // Assuming a line won't exceed 100 characters

        /* Check to see if file opened */
        if (fhA == NULL)
        {
            perror("Error opening scenario file");
            return;
        }
        
    }

    /* Child process to continue as simulator */
    if(child_pid == 0){
        return;
    }

    /* Confirm successful creation of child process */
    if (child_pid == -1)
    {
        perror("Fork failed");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "Missing command line arguments, {scenario file}\n");
        exit(1);
    }

    int serverPort = 3000;
    char serverAddress[16] = "127.0.0.1";

    initOverseer(argv[1], serverAddress, serverPort);
    init(argv[1]);

    return 0;
}