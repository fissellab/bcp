#include "telecommand_recv_server.hpp"
#include "recv_server.hpp"
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

TelecommandRecvServer::TelecommandRecvServer(udp::socket& listen_socket,
                                             Command& command,
                                             std::size_t buffer_size)
    : recv_server_(listen_socket,
                   std::bind(&TelecommandRecvServer::handle_message, this,
                             std::placeholders::_1),
                   buffer_size),
      command_(command) {};

void TelecommandRecvServer::handle_message(
    std::unique_ptr<std::vector<uint8_t>> message)
{
    std::string message_str(message->begin(), message->end());
    json telecommand = json::parse(message_str);
    std::cout << telecommand << std::endl;
    if(telecommand.contains("ack")) {
        std::vector<int32_t> seqnums_int =
            telecommand["ack"]["seqnums"].get<std::vector<int32_t>>();
        std::vector<uint32_t> seqnums(seqnums_int.begin(), seqnums_int.end());
        Ack ack = {.metric_id = telecommand["ack"]["metric_id"],
                   .sample_id = telecommand["ack"]["sample_id"],
                   .seqnums = seqnums};
        command_.handle_ack(std::move(ack));
    } else if(telecommand.contains("set_bps")) {
        command_.set_bps(telecommand["set_bps"]["bps"]);
        std::cout << "SET BPS" << std::endl;
    } else if(telecommand.contains("set_max_pkt_size")) {
        command_.set_max_packet_size(
            telecommand["set_max_pkt_size"]["max_pkt_size"]);
    } else {
        std::cerr << "Telecommand not recognized: " << message_str << std::endl;
    }
}
