/**
 * @file command_server.h
 * @brief A TCP server implementation for handling command-based communication
 *
 * This header file defines a command server that allows multiple clients to
 * connect and exchange commands over TCP. The server runs in a separate thread
 * and provides a pipe-based interface for the main application to receive
 * commands and broadcast messages to all connected clients.
 *
 * The server is designed to be thread-safe and supports multiple simultaneous
 * client connections. It uses a pipe for internal communication between the
 * server thread and the main application thread.
 *
 * ### Minimal example - Echo Server
 * @code
 * #include <stdio.h>
 * #include <string.h>
 * #include "command_server.h"
 *
 * int main(void) {
 *     command_server_t* server = command_server_create(8080, 10);
 *     if (!server) {
 *         fprintf(stderr, "Failed to create server\n");
 *         return 1;
 *     }
 *
 *     if (command_server_listen(server) != 0) {
 *         fprintf(stderr, "Failed to start server\n");
 *         command_server_destroy(server);
 *         return 1;
 *     }
 *
 *     printf("Echo server listening on port 8080...\n");
 *     printf("Send something to this server and it will be echoed back.\n");
 *     printf("Press Ctrl+C to exit.\n\n");
 *
 *     // Receive commands and echo them back
 *     char* cmd;
 *     while ((cmd = command_server_recv(server)) != NULL) {
 *         printf("Received: %s", cmd);
 *         // Echo the command back to all clients
 *         command_server_broadcast(server, cmd);
 *         free(cmd);
 *     }
 *
 *     command_server_destroy(server);
 *     return 0;
 * }
 * @endcode
 *
 * ### Example of polling stdin and command server simultaneously
 * @code
 * #include <stdio.h>
 * #include <sys/select.h>
 * #include <unistd.h>
 * #include "command_server.h"
 *
 * int main(void) {
 *     command_server_t* server = command_server_create(8080, 10);
 *     if (!server || command_server_listen(server) != 0) {
 *         // Error handling...
 *         return 1;
 *     }
 *
 *     fd_set readfds;
 *     struct timeval tv;
 *     char stdin_buf[1024];
 *
 *     while (1) {
 *         FD_ZERO(&readfds);
 *         FD_SET(STDIN_FILENO, &readfds);
 *         FD_SET(server->commands_read_fd, &readfds);
 *
 *         tv.tv_sec = 0;
 *         tv.tv_usec = 100000; // 100ms timeout
 *
 *         int ready = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);
 *
 *         if (ready < 0) {
 *             // Error handling...
 *             break;
 *         }
 *
 *         if (ready == 0) {
 *             // Timeout - continue polling
 *             continue;
 *         }
 *
 *         if (FD_ISSET(STDIN_FILENO, &readfds)) {
 *             if (fgets(stdin_buf, sizeof(stdin_buf), stdin)) {
 *                 // Handle stdin input
 *                 printf("Stdin: %s", stdin_buf);
 *             }
 *         }
 *
 *         if (FD_ISSET(server->commands_read_fd, &readfds)) {
 *             char* cmd = command_server_recv(server);
 *             if (cmd) {
 *                 // Handle command
 *                 printf("Command: %s\n", cmd);
 *                 free(cmd);
 *             }
 *         }
 *     }
 *
 *     command_server_destroy(server);
 *     return 0;
 * }
 * @endcode
 */

/**
 * @example gtest/command_server.cpp
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

// Structure to hold a client connection
typedef struct {
    int fd;                  // Client socket file descriptor
    struct sockaddr_in addr; // Client address information
} client_connection_t;

// Structure to hold the command server state
typedef struct {
    int socket_fd;                    // Server socket file descriptor
    int commands_read_fd;             // Read end of commands pipe
    int commands_write_fd;            // Write end of commands pipe
    client_connection_t* connections; // Array of client connections
    size_t num_connections;           // Number of active connections
    size_t max_connections;           // Maximum number of connections allowed
    pthread_t thread_id;              // Server thread ID
    int running;                      // Flag to control server thread
    uint16_t port;                    // Port the server is listening on
} command_server_t;

/**
 * @brief Creates and initializes a new command server
 *
 * @param port The port number to listen on
 * @param max_connections Maximum number of simultaneous connections
 * @return Pointer to the created server, or NULL on error
 */
command_server_t* command_server_create(uint16_t port, size_t max_connections);

/**
 * @brief Starts the command server listening thread
 *
 * @param server Pointer to the command server
 * @return 0 on success, -1 on error
 */
int command_server_listen(command_server_t* server);

/**
 * @brief Reads oldest unread command. Blocks until a command is available.
 *
 * @param server Pointer to the command server
 * @return Dynamically allocated string containing the command, or NULL on
 * error. The caller is responsible for freeing the returned string.
 */
char* command_server_recv(command_server_t* server);

/**
 * @brief Broadcasts a stdout message to all connected clients
 *
 * @param server Pointer to the command server
 * @param message The message to broadcast
 * @return int Returns 0 on success, -1 on error
 */
int command_server_broadcast(const command_server_t* server,
                             const char* message);

/**
 * @brief Cleans up and destroys the command server
 *
 * @param server Pointer to the command server
 */
void command_server_destroy(command_server_t* server);

#ifdef __cplusplus
}
#endif