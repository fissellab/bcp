#include "recv_server.hpp"
#include <boost/bind/bind.hpp>
#include <iostream>
#include <memory>
#include <string>

using boost::asio::ip::udp;

RecvServer::RecvServer(
    udp::socket& listen_socket,
    std::function<void(std::unique_ptr<std::vector<uint8_t>>)> message_handler,
    std::size_t buffer_size)
    : socket_(listen_socket), message_handler_(message_handler),
      buffer_size_(buffer_size),
      recv_buffer_(buffer_size) // initialize buffer with buffer_size bytes
{
    start_recv();
}

/**
 * async waits for incoming packets on socket_
 * When a packet is received, it is written to recv_buffer and
 * handle_recv is called
 */
void RecvServer::start_recv()
{
    // remote_endpoint recieves the endpoint of the sender
    // in async_receive_from. It is not used, but required to be passed
    // into the async_receive_from method.
    udp::endpoint remote_endpoint;

    // function to call handle_recv, passing it error code and
    // the number of bytes received
    auto on_recv = boost::bind(&RecvServer::handle_recv, this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred);

    socket_.async_receive_from(
        boost::asio::buffer(&recv_buffer_[0], buffer_size_), remote_endpoint,
        on_recv);
}

/**
 * passes bytes_received bytes of buffer to message_handler_
 */
void RecvServer::handle_recv(const boost::system::error_code& error,
                             std::size_t bytes_received /*bytes_transferred*/)
{
    // We will get error boost::asio::error::message_size
    // if the message is too big to fit in the buffer
    if(!error) {
        // char *received_data(recv_buffer_.data());
        // Must use unique ptr to give ownership to the caller
        std::unique_ptr<std::vector<uint8_t>> message =
            std::make_unique<std::vector<uint8_t>>(
                recv_buffer_.begin(), recv_buffer_.begin() + bytes_received);
        message_handler_(std::move(message));

    } else {
        std::cerr << "Error code" << error.to_string()
                  << "on receive msg: " << error.message() << std::endl;
    }

    // recurse
    start_recv();
}