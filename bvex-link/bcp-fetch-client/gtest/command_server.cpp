#include "command_server.h"
#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class CommandServerTest : public ::testing::Test
{
  protected:
    command_server_t* server;
    static const uint16_t TEST_PORT;
    static const size_t MAX_CONNECTIONS;

    void SetUp() override
    {
        server = command_server_create(TEST_PORT, MAX_CONNECTIONS);
        ASSERT_NE(server, nullptr);
        ASSERT_EQ(command_server_listen(server), 0);
    }

    void TearDown() override
    {
        if(server) {
            command_server_destroy(server);
        }
    }

    // Helper function to create a client socket and connect to the server
    int create_client_socket()
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_PORT);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            return -1;
        }
        return sock;
    }
};

// Define static members after the class declaration
const uint16_t CommandServerTest::TEST_PORT = 12345;
const size_t CommandServerTest::MAX_CONNECTIONS = 5;

// Test server creation
TEST_F(CommandServerTest, ServerCreation)
{
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(server->port, TEST_PORT);
    ASSERT_EQ(server->max_connections, MAX_CONNECTIONS);
    ASSERT_EQ(server->num_connections, 0);
}

// Test client connection
TEST_F(CommandServerTest, ClientConnection)
{
    int client_sock = create_client_socket();
    ASSERT_GE(client_sock, 0);

    // Give the server thread time to accept the connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(server->num_connections, 1);

    close(client_sock);
}

// Test message sending and receiving
TEST_F(CommandServerTest, MessageExchange)
{
    int client_sock = create_client_socket();
    ASSERT_GE(client_sock, 0);

    // Give the server thread time to accept the connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Send message from client
    const char* test_msg = "Hello, Server!";
    ASSERT_EQ(write(client_sock, test_msg, strlen(test_msg)), strlen(test_msg));

    // Receive message on server side
    char* received = command_server_recv(server);
    ASSERT_NE(received, nullptr);
    ASSERT_STREQ(received, test_msg);
    free(received);

    // Test broadcast
    const char* broadcast_msg = "Hello, Client!";
    ASSERT_EQ(command_server_broadcast(server, broadcast_msg), 0);

    // Receive broadcast on client side
    char buffer[1024];
    ASSERT_EQ(read(client_sock, buffer, sizeof(buffer)), strlen(broadcast_msg));
    buffer[strlen(broadcast_msg)] = '\0';
    ASSERT_STREQ(buffer, broadcast_msg);

    close(client_sock);
}

// Test multiple client connections
TEST_F(CommandServerTest, MultipleClients)
{
    const int NUM_CLIENTS = 3;
    int client_socks[NUM_CLIENTS];

    // Connect multiple clients
    for(int i = 0; i < NUM_CLIENTS; i++) {
        client_socks[i] = create_client_socket();
        ASSERT_GE(client_socks[i], 0);
    }

    // Give the server thread time to accept all connections
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(server->num_connections, NUM_CLIENTS);

    // Test broadcast to all clients
    const char* broadcast_msg = "Broadcast to all!";
    ASSERT_EQ(command_server_broadcast(server, broadcast_msg), 0);

    // Verify all clients received the message
    for(int i = 0; i < NUM_CLIENTS; i++) {
        char buffer[1024];
        ASSERT_EQ(read(client_socks[i], buffer, sizeof(buffer)),
                  strlen(broadcast_msg));
        buffer[strlen(broadcast_msg)] = '\0';
        ASSERT_STREQ(buffer, broadcast_msg);
        close(client_socks[i]);
    }
}

// Test maximum connections limit
TEST_F(CommandServerTest, MaxConnectionsLimit)
{
    const int NUM_CLIENTS = MAX_CONNECTIONS + 2;
    int client_socks[NUM_CLIENTS];

    // Try to connect more clients than the maximum
    for(int i = 0; i < NUM_CLIENTS; i++) {
        client_socks[i] = create_client_socket();
    }

    // Give the server thread time to process connections
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(server->num_connections, MAX_CONNECTIONS);

    // Clean up
    for(int i = 0; i < NUM_CLIENTS; i++) {
        if(client_socks[i] >= 0) {
            close(client_socks[i]);
        }
    }
}

// Test client disconnection
TEST_F(CommandServerTest, ClientDisconnection)
{
    int client_sock = create_client_socket();
    ASSERT_GE(client_sock, 0);

    // Give the server thread time to accept the connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(server->num_connections, 1);

    // Close client connection
    close(client_sock);

    // Give the server thread time to detect disconnection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_EQ(server->num_connections, 0);
}
