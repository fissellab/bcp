#pragma once

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "../src/generated/nanopb/primitive.pb.h"
#include "../src/generated/nanopb/request.pb.h"
#include "../src/generated/nanopb/response.pb.h"
#include <pb_decode.h>
#include <pb_encode.h>

class TestServer
{
  public:
    using RequestHandler = std::function<void(const Request&, Response&)>;

    TestServer(const std::string& host, int port)
        : host_(host), port_(port), running_(false)
    {
    }

    void setHandler(RequestHandler handler) { handler_ = handler; }

    bool isReady() const
    {
        if(server_fd_ < 0)
            return false;

        // Try to bind to the same address/port to check if it's in use
        int test_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(test_sock < 0)
            return false;

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(host_.c_str());
        addr.sin_port = htons(port_);

        bool ready =
            bind(test_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0;
        close(test_sock);
        return ready;
    }

    void waitUntilReady(
        std::chrono::milliseconds timeout = std::chrono::seconds(5)) const
    {
        auto start = std::chrono::steady_clock::now();
        while(!isReady()) {
            if(std::chrono::steady_clock::now() - start > timeout) {
                throw std::runtime_error(
                    "Server failed to become ready within timeout");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void start()
    {
        server_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if(server_fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        // Set socket to non-blocking mode
        int flags = fcntl(server_fd_, F_GETFL, 0);
        if(flags == -1) {
            close(server_fd_);
            throw std::runtime_error("Failed to get socket flags");
        }
        if(fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(server_fd_);
            throw std::runtime_error(
                "Failed to set socket to non-blocking mode");
        }

        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(host_.c_str());
        server_addr.sin_port = htons(port_);

        if(bind(server_fd_, (struct sockaddr*)&server_addr,
                sizeof(server_addr)) < 0) {
            close(server_fd_);
            throw std::runtime_error("Failed to bind socket");
        }

        running_ = true;
    }

    void stop()
    {
        running_ = false;
        if(server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }

    void run()
    {
        while(running_) {
            uint8_t buffer[REQUEST_PB_H_MAX_SIZE];
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            ssize_t bytes_received =
                recvfrom(server_fd_, buffer, sizeof(buffer), 0,
                         (struct sockaddr*)&client_addr, &client_len);

            // Check if we should exit
            if(!running_) {
                break;
            }

            if(bytes_received < 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available, continue loop
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                std::cerr << "Failed to receive data: " << strerror(errno)
                          << std::endl;
                continue;
            }

            Request request = Request_init_zero;
            pb_istream_t stream =
                pb_istream_from_buffer(buffer, bytes_received);
            if(!pb_decode(&stream, Request_fields, &request)) {
                std::cerr << "Failed to decode request: "
                          << PB_GET_ERROR(&stream) << std::endl;
                continue;
            }

            Response response = Response_init_zero;
            if(handler_) {
                handler_(request, response);
            }

            uint8_t response_buffer[RESPONSE_PB_H_MAX_SIZE];
            pb_ostream_t out_stream = pb_ostream_from_buffer(
                response_buffer, sizeof(response_buffer));
            if(!pb_encode(&out_stream, Response_fields, &response)) {
                std::cerr << "Failed to encode response: "
                          << PB_GET_ERROR(&out_stream) << std::endl;
                continue;
            }

            ssize_t bytes_sent =
                sendto(server_fd_, response_buffer, out_stream.bytes_written, 0,
                       (struct sockaddr*)&client_addr, client_len);
            if(bytes_sent < 0) {
                std::cerr << "Failed to send response: " << strerror(errno)
                          << std::endl;
            }
        }
    }

  private:
    std::string host_;
    int port_;
    bool running_;
    int server_fd_ = -1;
    RequestHandler handler_;
};