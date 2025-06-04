#include "command.hpp"
#include "server/onboard_telemetry_recv_server.hpp"
#include "server/request_server.hpp"
#include "server/send_server.hpp"
#include "server/telecommand_recv_server.hpp"
#include "telemetry.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <string>

#ifndef REQUEST_SERVER_PORT
#define REQUEST_SERVER_PORT "8080"
#endif

using boost::asio::ip::udp;

int main(int argc, char* argv[])
{
    try {
        if(argc != 3) {
            std::cerr << "Usage: " << argv[0]
                      << " <target_address> <target_port>\n";
            return 1;
        }

        std::string target_address_str = argv[1];
        boost::asio::ip::address target_address;
        if(target_address_str == "localhost") {
            // not IPv6 compatible, shouldn't be an issue
            target_address = boost::asio::ip::make_address("127.0.0.1");
        } else {
            target_address = boost::asio::ip::make_address(argv[1]);
        }
        const boost::asio::ip::port_type target_port =
            static_cast<uint_least16_t>(std::atoi(argv[2]));

        const boost::asio::ip::port_type onboard_telemetry_recv_port = 3000;
        const boost::asio::ip::port_type telecommand_recv_port = 3001;
        const boost::asio::ip::port_type send_port = 3002;
        const boost::asio::ip::port_type requests_port =
            static_cast<boost::asio::ip::port_type>(
                std::stoi(REQUEST_SERVER_PORT));

        if(target_port == onboard_telemetry_recv_port ||
           target_port == telecommand_recv_port || target_port == send_port ||
           requests_port == send_port) {
            std::cerr << "Target port cannot be the same as any of the server "
                         "ports\n";
            return 1;
        }
        Command command = Command(100000, 100);
        Telemetry telemetry = Telemetry(command);
        boost::asio::io_service io_service;

        udp::socket onboard_telemetry_listen_socket(
            io_service, udp::endpoint(udp::v4(),
            onboard_telemetry_recv_port));
        // enable SO_REUSEADDR to fix address already in use after crash
        onboard_telemetry_listen_socket.set_option(
            boost::asio::socket_base::reuse_address(true));
        OnboardTelemetryRecvServer onboard_telemetry_recv_server(
            onboard_telemetry_listen_socket, command);

        udp::socket requests_socket(io_service,
                                    udp::endpoint(udp::v4(), requests_port));
        // enable SO_REUSEADDR to fix address already in use after crash
        requests_socket.set_option(
            boost::asio::socket_base::reuse_address(true));
        RequestServer request_server(
            requests_socket, std::bind(&Command::get_latest_sample_response,
                                       &command, std::placeholders::_1));

        udp::socket telecommand_listen_socket(
            io_service, udp::endpoint(udp::v4(), telecommand_recv_port));
        // enable SO_REUSEADDR to fix address already in use after crash
        telecommand_listen_socket.set_option(
            boost::asio::socket_base::reuse_address(true));
        TelecommandRecvServer
        telecommand_recv_server(telecommand_listen_socket,
                                                      command);

        udp::socket send_socket(io_service,
                                udp::endpoint(udp::v4(), send_port));
        // enable SO_REUSEADDR to fix address already in use after crash
        send_socket.set_option(boost::asio::socket_base::reuse_address(true));
        udp::endpoint target_endpoint(target_address, target_port);
        SendServer send_server(io_service, send_socket, target_endpoint,
                               telemetry, command);
                               
        io_service.run();
    } catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}