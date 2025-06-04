#include "command_server.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if(argc < 2) {
        printf("Usage: %s <port> - Start a example command server on specified "
               "port\n",
               argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if(port <= 0 || port > 65535) {
        printf("Error: invalid port number\n");
        return 1;
    }

    command_server_t* server = command_server_create(port, 10);
    if(!server) {
        printf("Failed to create command server\n");
        return 1;
    }

    if(command_server_listen(server) < 0) {
        printf("Failed to start command server\n");
        command_server_destroy(server);
        return 1;
    }

    printf("Command server listening on port %d\n", port);

    struct pollfd fds[1];
    fds[0].fd = server->commands_read_fd;
    fds[0].events = POLLIN;

    while(1) {
        int ret = poll(fds, 1, 1000); // Poll with 1 second timeout
        if(ret < 0) {
            printf("Poll error\n");
            break;
        }

        if(fds[0].revents & POLLIN) {
            char* cmd = command_server_recv(server);
            printf("Received command: %s\n", cmd);
            if(cmd) {
                if(strcmp(cmd, "ping") == 0) {
                    printf("Broadcasting pong!\n");
                    command_server_broadcast(server, "pong!");
                } else if(strcmp(cmd, "time") == 0) {
                    printf("Broadcasting time\n");
                    time_t now = time(NULL);
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
                             localtime(&now));
                    command_server_broadcast(server, time_str);
                } else {
                    printf("Broadcasting help\n");
                    // Default help message
                    command_server_broadcast(
                        server, "Available commands:\n"
                                "  ping - Server responds with 'pong!'\n"
                                "  time - Get current server time\n"
                                "  help - Show this message");
                }
                free(cmd);
            }
        }
    }

    return 0;
}