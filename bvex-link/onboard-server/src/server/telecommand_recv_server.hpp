#pragma once

#include "../command.hpp"
#include "recv_server.hpp"
#include <boost/asio.hpp>
#include <memory>

class TelecommandRecvServer
{
  public:
    TelecommandRecvServer(udp::socket& listen_socket, Command& command,
                          std::size_t buffer_size = 4096);

  private:
    void handle_message(std::unique_ptr<std::vector<uint8_t>> message);
    RecvServer recv_server_;
    Command& command_;
};