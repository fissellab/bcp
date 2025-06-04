#include "test_client.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if(argc < 3) {
        printf("Usage: %s <host> <port>\n", argv[0]);
        printf("Starts an interactive command session with the server\n");
        return 1;
    }

    char* host = argv[1];
    int port = atoi(argv[2]);

    if(port <= 0 || port > 65535) {
        printf("Error: invalid port number\n");
        return 1;
    }

    interactive_session(host, port);
    return 0;
}