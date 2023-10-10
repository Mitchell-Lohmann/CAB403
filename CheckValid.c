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
const char *cardID = "101"; // Will take input from other functions within overseer

// Function declaration
bool checkValid(const char *cardSearch, const char *cardID);

/* Main loop to test function*/
int main() {

    printf("Valid card, valid card reader \n");
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
    char *doorID = NULL;

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
                doorID = strdup(token); // Allocate memory and copy the value
                break;
            }
        }
    }

    if (doorID == NULL){
        printf("Could not match card reader ID with door ID \n"); // Debug line
        free(doorID);
        fclose(fhA);
        fclose(fhB);
        return false;
    }

    // Read the file line by line
    while (fgets(lineB, sizeof(lineB), fhA)) {
        // Remove newline character if present
        size_t len = strlen(lineB);
        if (len > 0 && lineB[len - 1] == '\n') {
            lineB[len - 1] = '\0'; 
        }

        // Check if the card number is found in the line
        if (strstr(lineB, cardSearch) != NULL) {
            // If found check if the door is also found in the same line
            if (strcmp(lineB, doorID) != 0) {
                /* Card number was found in authorisation file with access to desired door. Close file and return true */
                free(doorID);
                fclose(fhA);
                fclose(fhB);
                return true; 
            }
        }
    }

    /* Card number was not found in authorisation file. Close file and return false */
    free(doorID);
    fclose(fhA);
    fclose(fhB);
    return false;
}
