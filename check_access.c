#include <stdio.h>
#include <string.h>
#include <stdbool.h>

const char *userId = "db4ed0a0bfbb00ac"; // Will take input from other functions within overseer
const char *card_reader_id = "102";

void check_access(const char *userId, const char *card_reader_id) {
    FILE *file = fopen("authorisation.txt", "r");
    if (file == NULL) {
        perror("Failed to open the authentication file");
        return;
    }

    char line[256]; // Assuming a line in the file is not longer than 256 characters
    int DoorList[5]; // Assuming a user only has access to 5 doors
    char *Door;


    // Read each line of the file
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, " "); // Split the line by space

        // Check if the first token is the user ID we're looking for
        if (strcmp(token, userId) == 0) {
            printf("User ID: %s\n", userId);

            // Read and print the doors to which the user has access
            while ((token = strtok(NULL, " "))) {
                if (strncmp(token, "DOOR:", 5) == 0) {
                    printf("Door Access: %s\n", token + 5);
                }
            }

            break; // No need to continue reading the file
        }
    }
    fclose(file);

    FILE *file2 = fopen("connections.txt", "r");
    if (file2 == NULL) {
        perror("Failed to open the authentication file");
        return;
    }


    // Read each line of the file
    while (fgets(line, sizeof(line), file2)) {
        char *token = strtok(line, " "); // Split the line by space

        // Check if the first token is DOOR
        if (strcmp(token, "DOOR") == 0) {
            printf("Card reader ID: %s\n", card_reader_id );
            token = strtok(NULL, " "); // Split the line by space
            if (strcmp(token, card_reader_id) == 0)
            {
                token = strtok(NULL, " "); // Split the line by space

                Door = token;
                printf("Door associated with card reader is %s", Door);
                break;
            }
        }
    }
    fclose(file);
}

int main() {
    
    check_access(userId, card_reader_id);

    return 0;
}