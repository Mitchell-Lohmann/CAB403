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

            if (child_pid == 0)
            {
                char *argument1 = NULL, *argument4 = NULL, *argument5 = NULL, *argument6 = NULL;
                int *argument2 = 0, *argument3 = 0;

                

                if (!strcmp(token, "overseer"))
                {
                    /* Check that sscanf is successful */
                    if(sscanf(lineA, "INIT overseer %d %d %s %s %s", argument2, argument3, argument4, argument5, argument6) != 5){
                        perror("sscanf failed");
                    }

                    argument1 = "overseer";
                    printf("overseer executed\n");
                    execl(argument1,argument1, argument2, argument3, argument4, argument5, argument6, NULL);
                    // Fork
                    // Replace child process with overseer process
                }
                else if (!strcmp(token, "door"))
                {
                    printf("door found in line %s\n", lineA);
                }
                else if (!strcmp(token, "cardreader"))
                {
                    printf("cardreader found in line %s\n", lineA);
                }
                else if (!strcmp(token, "firealarm"))
                {
                    printf("firealarm found in line %s\n", lineA);
                }
                else if (!strcmp(token, "callpoint"))
                {
                    printf("callpoint found in line %s\n", lineA);
                }
                else if (!strcmp(token, "tempsensor"))
                {
                    printf("tempsensor found in line %s\n", lineA);
                }
                else if (!strcmp(token, "destselect"))
                {
                    printf("destselect found in line %s\n", lineA);
                }
                else if (!strcmp(token, "camera"))
                {
                    printf("camera found in line %s\n", lineA);
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