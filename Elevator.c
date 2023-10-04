#include <stdio.h>
#include <string.h>

int main()
{
    char overseer_addr[50] = "127.0.0.1:4001";
    const char * addr = strtok(overseer_addr, ":");
    const char * port = strtok(NULL, ";");
    printf( " %s\n", addr );
    printf( " %s\n", port );
}
