#include <arpa/inet.h>
#include <connected_udp_socket.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Helper function to create a test UDP server
static int create_test_server(const char* port)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1) {
        return -1;
    }

    struct sockaddr_in other_addr;
    memset(&other_addr, 0, sizeof(other_addr));
    other_addr.sin_family = AF_INET;
    other_addr.sin_addr.s_addr = INADDR_ANY;
    other_addr.sin_port = htons(atoi(port));

    if(bind(sockfd, (struct sockaddr*)&other_addr, sizeof(other_addr)) ==
       -1) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

TEST(ConnectedUdpSocket, BasicFunctionality)
{
    // Create an other socket
    const char* port = "8080";
    int other_fd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(other_fd, -1);
    struct sockaddr_in other_addr;
    memset(&other_addr, 0, sizeof(other_addr));
    other_addr.sin_family = AF_INET;
    other_addr.sin_addr.s_addr = INADDR_ANY;
    other_addr.sin_port = htons(atoi(port));
    int bind_result = bind(other_fd, (struct sockaddr*)&other_addr, sizeof(other_addr));
    ASSERT_NE(bind_result, -1);

    // Create a connected udp socket
    int connected_udp_fd = connected_udp_socket("localhost", "8080");
    ASSERT_NE(connected_udp_fd, -1);

    // Test sending data from connected socket
    const char* test_msg = "Hello, World!";
    ssize_t sent = send(connected_udp_fd, test_msg, strlen(test_msg), 0);
    EXPECT_NE(sent, -1);

    // Test receiving data from other socket
    char buffer[1024];
    socklen_t addr_len = sizeof(struct sockaddr_in);
    ssize_t received = recvfrom(other_fd, buffer, sizeof(buffer), 0,
                              (struct sockaddr*)&other_addr, &addr_len);
    EXPECT_NE(received, -1);
    buffer[received] = '\0';  // Null terminate the received string
    EXPECT_EQ(strcmp(buffer, test_msg), 0);

    // Clean up
    close(connected_udp_fd);
    close(other_fd);
}

TEST(ConnectedUdpSocket, InvalidHost)
{
    int sockfd = connected_udp_socket("nonexistent_host", "8080");
    EXPECT_EQ(sockfd, -1);
}

TEST(ConnectedUdpSocket, InvalidPort)
{
    int sockfd = connected_udp_socket("localhost", "invalid_port");
    EXPECT_EQ(sockfd, -1);
}

TEST(ConnectedUdpSocket, NullParameters)
{
    int sockfd = connected_udp_socket(NULL, "8080");
    EXPECT_EQ(sockfd, -1);

    sockfd = connected_udp_socket("localhost", NULL);
    EXPECT_EQ(sockfd, -1);

    sockfd = connected_udp_socket(NULL, NULL);
    EXPECT_EQ(sockfd, -1);
}
