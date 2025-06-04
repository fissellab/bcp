#include "request_sample.h"
#include "../test_common/test_server.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

class RequestSampleTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Start test server in a separate thread
        server = std::make_unique<TestServer>("127.0.0.1", 3000);

        // Start server thread
        server_thread = std::thread([this]() {
            try {
                server->start();
                server->run();
            } catch(const std::exception& e) {
                std::cerr << "Server error: " << e.what() << std::endl;
            }
        });

        // Wait for server to be ready
        server->waitUntilReady();

        // Verify server is running by checking if port is in use
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(3000);

        if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            std::cerr << "Warning: Server port not in use after initialization"
                      << std::endl;
            close(sock);
        }
        close(sock);
    }

    void TearDown() override
    {
        if(server) {
            server->stop();
        }
        if(server_thread.joinable()) {
            server_thread.join();
        }
    }

    std::unique_ptr<TestServer> server;
    std::thread server_thread;
};

TEST_F(RequestSampleTest, RequestInt)
{
    // Set up server to return integer value
    server->setHandler([](const Request& request, Response& response) {
        strcpy(response.metric_id, request.metric_id);
        response.has_primitive = true;
        response.primitive.which_value = primitive_Primitive_int_val_tag;
        response.primitive.value.int_val = 42;
    });

    Requester requester = make_requester("test_metric", "127.0.0.1", "3000");
    ASSERT_GE(requester.socket_fd, 0) << "Failed to create requester";

    RequestIntResult result = request_int(&requester);

    EXPECT_EQ(result.err, REQUEST_STATUS_OK);
    if(!result.err) {
        EXPECT_EQ(result.value, 42);
    }
}

TEST_F(RequestSampleTest, RequestFloat)
{
    // Set up server to return float value
    server->setHandler([](const Request& request, Response& response) {
        response.has_primitive = true;
        response.primitive.which_value = primitive_Primitive_float_val_tag;
        response.primitive.value.float_val = 3.14f;
    });

    Requester requester = make_requester("test_metric", "127.0.0.1", "3000");
    ASSERT_GE(requester.socket_fd, 0);

    RequestFloatResult result = request_float(&requester);
    EXPECT_EQ(result.err, REQUEST_STATUS_OK);
    if(!result.err) {
        EXPECT_FLOAT_EQ(result.value, 3.14f);
    }
}

TEST_F(RequestSampleTest, RequestDouble)
{
    // Set up server to return double value
    server->setHandler([](const Request& request, Response& response) {
        response.has_primitive = true;
        response.primitive.which_value = primitive_Primitive_double_val_tag;
        response.primitive.value.double_val = 3.14159;
    });

    Requester requester = make_requester("test_metric", "127.0.0.1", "3000");
    ASSERT_GE(requester.socket_fd, 0);

    RequestDoubleResult result = request_double(&requester);
    EXPECT_EQ(result.err, REQUEST_STATUS_OK);
    if(!result.err) {
        EXPECT_DOUBLE_EQ(result.value, 3.14159);
    }
}

TEST_F(RequestSampleTest, RequestString)
{
    // Set up server to return string value
    server->setHandler([](const Request& request, Response& response) {
        response.has_primitive = true;
        response.primitive.which_value = primitive_Primitive_string_val_tag;
        strcpy(response.primitive.value.string_val, "test_string");
    });

    Requester requester = make_requester("test_metric", "127.0.0.1", "3000");
    ASSERT_GE(requester.socket_fd, 0);

    RequestStringResult result = request_string(&requester);
    EXPECT_EQ(result.err, REQUEST_STATUS_OK);
    if(!result.err) {
        EXPECT_STREQ(result.value, "test_string");
        free(result.value); // Clean up allocated memory
    }
}

TEST_F(RequestSampleTest, InvalidResponseType)
{
    // Set up server to return wrong type (string when requesting int)
    server->setHandler([](const Request& request, Response& response) {
        response.has_primitive = true;
        response.primitive.which_value = primitive_Primitive_string_val_tag;
        strcpy(response.primitive.value.string_val, "wrong_type");
    });

    Requester requester = make_requester("test_metric", "127.0.0.1", "3000");
    ASSERT_GE(requester.socket_fd, 0);

    RequestIntResult result = request_int(&requester);
    EXPECT_EQ(result.err, REQUEST_STATUS_INVALID_RESPONSE_TYPE);
}

TEST_F(RequestSampleTest, ServerNotRunning)
{
    // Stop the server before making request
    server->stop();
    if(server_thread.joinable()) {
        server_thread.join();
    }

    Requester requester = make_requester("test_metric", "127.0.0.1", "3000");
    ASSERT_GE(requester.socket_fd, 0);

    RequestIntResult result = request_int(&requester);
    EXPECT_NE(result.err,
              REQUEST_STATUS_OK); // Should fail with send/recv error
}
