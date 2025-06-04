#include "test_client.h"
#include "command_server.h"
#include "common.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_SERVER_ADDR "127.0.0.1"

// Helper function to create a client socket with custom host and port
static int create_client_socket_with_params(const char* host, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("Failed to create socket");
        return -1;
    }

    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int rv;
    if((rv = getaddrinfo(host, port_str, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        close(sock);
        return -1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if(connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if(p == NULL) {
        fprintf(stderr, "Failed to connect\n");
        close(sock);
        return -1;
    }

    return sock;
}

// Interactive session with server that handles both user input and broadcasts
void interactive_session(const char* host, int port)
{
    int sock = create_client_socket_with_params(host, port);
    if(sock < 0) {
        printf("Failed to connect to server at %s:%d\n", host, port);
        return;
    }

    printf("Connected to command server at %s:%d\n", host, port);
    printf("Enter commands (type 'exit' to quit):\n");

    struct pollfd fds[2];

    // Monitor stdin
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // Monitor socket
    fds[1].fd = sock;
    fds[1].events = POLLIN;

    char buffer[1024];
    int running = 1;

    while(running) {
        printf("> ");
        fflush(stdout);

        int ret = poll(fds, 2, -1); // Wait indefinitely for events
        if(ret < 0) {
            perror("Poll failed");
            break;
        }

        // Check for socket data (broadcasts from server)
        if(fds[1].revents & POLLIN) {
            ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if(bytes <= 0) {
                if(bytes == 0) {
                    printf("\nServer connection closed\n");
                } else {
                    perror("\nFailed to receive from server");
                }
                break;
            }
            buffer[bytes] = '\0';
            printf("\nServer: %s\n> ", buffer);
            fflush(stdout);
        }

        // Check for user input
        if(fds[0].revents & POLLIN) {
            if(fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }

            // Remove trailing newline
            buffer[strcspn(buffer, "\n")] = 0;

            if(strcmp(buffer, "exit") == 0) {
                running = 0;
                continue;
            }

            // Send command to server
            if(send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("Failed to send command");
                break;
            }
        }
    }

    close(sock);
}