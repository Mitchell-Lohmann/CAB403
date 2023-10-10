#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
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

const char *cardSearch = "db4ed0a0bfbb00ac"; // Will take input from other functions within overseer
const char *cardID = "103"; // Will take input from other functions within overseer

// Function declaration
bool checkValid(const char *cardSearch, const char *cardID);

/* Main loop to test function*/
int main() {

    if (checkValid(cardSearch, cardID)) {
        printf("Access Granted: User with card number '%s' is granted access through card reader '%s'.\n", cardSearch, cardID);
        // Code for valid card
    } else {
        printf("Access Denied: User with card number '%s' is denied access through card reader '%s'.\n", cardSearch, cardID);
        // Code for invalid card
    }
    return 0; 
}

// <summary>
// Function to validate card number and access permission to desired door. Returns true if card is valid at desired door, returns false if card is invalid at desired door.
// </summary>
bool checkValid(const char *cardSearch, const char *cardID) {

    FILE *fhA = fopen("authorisation.txt", "r");
    FILE *fhB = fopen("connections.txt", "r");
    char lineA[100];  // Assuming a line won't exceed 100 characters
    char lineB[100];  // Assuming a line won't exceed 100 characters
    int doorID = -1;

    if (fhA == NULL) {
        perror("Error opening authorisation file");
        return false;
    }

    if (fhB == NULL) {
        perror("Error opening connections file");
        return false;
    }
        
    while (fgets(lineA, sizeof(lineA), fhB)) {
        char *token = strtok(lineA, " "); // Split the line by space

        // Check if the first token is DOOR
        if (strcmp(token, "DOOR") == 0) {
            token = strtok(NULL, " "); // Split the line by space
            if (strcmp(token, cardID) == 0)
            {
                token = strtok(NULL, " "); // Split the line by space
                doorID = atoi(token); // Allocate memory and copy the value
                break;
            }
        }
    }

    if (doorID == -1){
        printf("Could not match card reader ID with door ID \n"); // Debug line
        fclose(fhA);
        fclose(fhB);
        return false;
    }

    // Read the file line by line
    while (fgets(lineB, sizeof(lineB), fhA))
    {
        char *token = strtok(lineB, " "); // Split the line by space
        // Check if the first token is the user ID we're looking for
        if (strcmp(token, cardSearch) == 0)
        {
            // Read and print the doors to which the user has access
            while ((token = strtok(NULL, " ")))
            {
                if (strncmp(token, "DOOR:", 5) == 0)
                {
                    if (doorID == atoi(token + 5))
                    {
                        fclose(fhA);
                        fclose(fhB);
                        return true;
                    }
                }
            }

            break; // No need to continue reading the file
        }
    }

    /* Card number was not found in authorisation file. Close file and return false */
    fclose(fhA);
    fclose(fhB);
    return false;
}
