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

typedef struct
{ 
    char scanned[16];
    pthread_mutex_t mutex;
    pthread_cond_t scanned_cond;

    char response; // 'Y' or 'N' (or '\0' at first)
    pthread_cond_t response_cond;
} shm_cardreader;

/// <summary>
/// Function takes input of scenario file name and runs all initialisation setup
/// commands. Returns no value. Errors if scenario file is not valid.
/// </summary>
void init(char *scenarioName)
{
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

            char argument0[64], argument1[64], argument2[64], argument3[64], argument4[46], argument5[64], argument6[64];

            if (child_pid == 0)
            {
                

                if (!strcmp(token, "overseer"))
                {
                    printf("overseer found in line %s\n", lineA); // Debug line
                    /* Check that sscanf is successful */
                    if(sscanf(lineA, "INIT overseer %s %s %s %s %s", argument2, argument3, argument4, argument5, argument6) != 5){
                        perror("sscanf failed");
                        exit(1);
                    }
                    printf("Sscanf returned: %d %d %s %s %s \n", atoi(argument2), atoi(argument3), argument4, argument5, argument6); // Debug line

                    strcpy(argument0, "./overseer");
                    printf("overseer executed\n"); // Debug line
                    execl(argument0, argument1, argument2, argument3, argument4, argument5, argument6, NULL);
                    perror("execl");
                    
                }
                else if (!strcmp(token, "door"))
                {
                    //printf("door found in line %s\n", lineA);
                     /* Check that sscanf is successful */
                    if(sscanf(lineA, "INIT door %s %s %s", argument2, argument3, argument4) != 3){
                        perror("sscanf failed");
                        exit(1);
                    }

                    printf("Sscanf returned: %d %s %d\n", atoi(argument2), argument3, atoi(argument4)); // Debug line

                    strcpy(argument1, "door");
                    execl(argument1, argument1, argument2, argument3, argument4, NULL);
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
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "Missing command line arguments, {scenario file}\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        printf("Argument %d: %s\n", i, argv[i]);
    }

    init(argv[1]);

    return 0;
}