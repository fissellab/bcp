/**
 * @file connected_udp_socket.h
 * @brief Header file for creating connected UDP sockets
 *
 * This header provides a function to create a UDP socket that are
 * connected to a specific endpoint. This should be used to create a
 * socket for the send sample functions in send_sample.h.
 *
 * ### Example usage
 * @code
 * #include <stdio.h>
 * #include <string.h>
 * #include <unistd.h>
 * #include "connected_udp_socket.h"
 *
 * int main(void) {
 *     // Create a UDP socket connected to localhost:8080
 *     int sockfd = connected_udp_socket("localhost", "8080");
 *     if (sockfd == -1) {
 *         perror("Failed to create socket");
 *         return 1;
 *     }
 *
 *     // Send "Hello, World!" to the connected endpoint
 *     const char *msg = "Hello, World!";
 *     ssize_t sent = send(sockfd, msg, strlen(msg), 0);
 *     if (sent == -1) {
 *         perror("Failed to send message");
 *         close(sockfd);
 *         return 1;
 *     }
 *
 *     // Clean up
 *     close(sockfd);
 *     return 0;
 * }
 * @endcode
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @example gtest/connected_udp_socket.cpp
 */

/**
 * @brief Makes UDP socket, connects to node:service, and returns socket file
 * descriptor.
 *
 * This function creates a UDP socket, connects it to the specified node and
 * service, and returns the socket file descriptor. The socket is not bound to a
 * specific port, allowing the OS to choose. The socket's endpoint is set to the
 * specified node and service using connect(), allowing you to send data to the
 * same address with send().
 *
 * @param node IP address or hostname.
 * @param service Port number or service name (e.g., "1234" or "http").
 * @return Socket file descriptor on success, or -1 on error.
 *
 * @note To close the socket, use close(fd), where fd is the socket's file
 * descriptor.
 */
int connected_udp_socket(const char* node, const char* service);

#ifdef __cplusplus
}
#endif
