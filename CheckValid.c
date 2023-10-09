#include <stdio.h>
#include <string.h>
#include <stdbool.h>

const char *cardSearch = "db4ed0a0bfbb00ac"; // Will take input from other functions within overseer
const char *doorSearch = "DOOR:101"; // Will take input from other functions within overseer

// Function declaration
bool checkValid(const char *cardSearch, const char *doorSearch);

/* Main loop to test function*/
int main() {

    if (checkValid(cardSearch, doorSearch)) {
        printf("Access Granted: User with card number '%s' is granted access to door '%s'.\n", cardSearch, doorSearch);
        // Code for valid card
    } else {
        printf("Access Denied: User with card number '%s' is denied access to door '%s'.\n", cardSearch, doorSearch);
        // Code for invalid card
    }

    return 0;
}

// <summary>
// Function to validate card number and access permission to desired door. Returns true if card is valid at desired door, returns false if card is invalid at desired door.
// </summary>
bool checkValid(const char *cardSearch, const char *doorSearch) {
    FILE *fh = fopen("authorisation.txt", "r");
    char line[100];  // Assuming a line won't exceed 100 characters

    if (fh == NULL) {
        perror("Error opening file");
        return false;
    }

    // Read the file line by line
    while (fgets(line, sizeof(line), fh)) {
        // Remove newline character if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Check if the card number is found in the line
        if (strstr(line, cardSearch) != NULL) {
            // If found check if the door is also found in the same line
            if (strstr(line, doorSearch) != NULL) {
                /* Card number was found in authorisation file with access to desired door. Close file and return true */
                fclose(fh);
                return true; 
            }
        }
    }

    /* Card number was not found in authorisation file. Close file and return false */
    fclose(fh);
    return false;
}
