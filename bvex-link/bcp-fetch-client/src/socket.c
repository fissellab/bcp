#include "connected_udp_socket.h"
#include <arpa/inet.h>  // htons, inet_addr
#include <fcntl.h>      // fcntl
#include <netdb.h>      // addrinfo, getaddrinfo
#include <netinet/in.h> // sockaddr_in
#include <stdio.h>
#include <string.h>     // strcmp
#include <sys/socket.h> // socket, connect
#include <unistd.h>     // close

int connected_udp_socket(const char* node, const char* service)
{
    // Validate input parameters
    if(!node || !service) {
        return -1;
    }

    // --- GET LINKED LIST servinfo OF addrinfo STRUCTS CONTAINING
    // --- ADDRESS INFORMATION FOR node:service ---

    struct addrinfo hints, *servinfo;

    // fill in hints struct
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;  // UDP sockets

    int status; // return code for getaddrinfo
    // set linked-list of addrinfo structs to servinfo
    status = getaddrinfo(node, service, &hints, &servinfo);
    if(status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    // --- LOOP THROUGH servinfo AND CONNECT TO FIRST SUCCESSFUL RESULT ---

    int sockfd = -1;    // socket file descriptor
    struct addrinfo* p; // pointer to iterate through servinfo
    // loop through all the results and connect to the first we can
    // begin with head of servinfo linked list
    int connect_status; // status of connect() call
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // attempt to create socket
        sockfd = socket(p->ai_family, SOCK_DGRAM, p->ai_protocol);
        // if socket creation fails, print error and continue to next node of
        // servinfo
        if(sockfd == -1) {
            perror("client: socket");
            continue;
        }

        connect_status = connect(sockfd, p->ai_addr, p->ai_addrlen);
        // if connection fails, print error, close socket and continue to next
        // node of servinfo
        if(connect_status == -1) {
            close(sockfd);
            sockfd = -1;
            perror("client: connect");
            continue;
        } else {
            // if we get here, we must have connected successfully, so stop
            // looping
            // through servinfo
            break;
        }
    }

    // free the linked-list now that we have connected socket
    freeaddrinfo(servinfo);

    // if p is NULL, then we have looped through all servinfo nodes and failed
    if(p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        close(sockfd);
        return -1;
    }

    // now a socket description exists. If it is not bound to a specific port
    // using bind() before its first send() or sendto() call, then the OS will
    // assign a random port number.

    return sockfd;
}
