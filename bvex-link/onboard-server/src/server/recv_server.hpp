#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <memory>
#include <vector>

#include "../command.hpp"

using boost::asio::ip::udp;

class RecvServer
{
  public:
    RecvServer(udp::socket& listen_socket,
               std::function<void(std::unique_ptr<std::vector<uint8_t>>)>
                   message_handler,
               std::size_t buffer_size = 4096);

  private:
    void start_recv();
    void handle_recv(const boost::system::error_code& error,
                     std::size_t bytes_recvd);

    udp::socket& socket_;
    std::function<void(std::unique_ptr<std::vector<uint8_t>>)> message_handler_;
    std::size_t buffer_size_;
    std::vector<uint8_t> recv_buffer_;
};
