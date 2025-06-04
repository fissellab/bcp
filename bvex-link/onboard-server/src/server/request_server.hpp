#pragma once

#include "../command.hpp"
#include "../telemetry.hpp"
#include <boost/asio.hpp>
#include <codec/requests/request.hpp>
#include <functional>
#include <memory>
#include <string>

using boost::asio::ip::udp;

/*!
   Send loop:
   \dot
   digraph send_loop {
        RequestServer -> start_recv;

        start_recv -> handle_recv;

        handle_recv -> start_recv;
   }
   \enddot
 */

/**
 * @brief Awaits requests and serves responses.
 *
 *
 */
class RequestServer
{
  public:
    /**
     * @brief Construct a new RequestServer object
     *
     * @param io_service The io_service to use for async network IO operations.
     * @param port The port to recv and send from.
     * @param command The command object to get the bps from.
     */
    RequestServer(udp::socket& listen_socket,
                  std::function<std::optional<std::vector<uint8_t>>(
                      std::string metric_id)>
                      get_latest_sample_response);

  private:
    udp::socket& socket_;
    udp::endpoint requester_endpoint_;
    std::function<std::optional<std::vector<uint8_t>>(std::string metric_id)>
        get_latest_sample_response_;
    std::vector<uint8_t> recv_buffer_;

    /**
     * @brief Async await incoming packets, specifying the data to be read to
     * recv_buffer_ and to call handle_recv on datagram received.
     *
     */
    void start_recv();

    /**
     * @brief Handles the data received by decoding it to a request and passing
     * that to handle_request
     *
     * Reads out errors if there were any, then calls start_send().
     *
     * @param message Shared ptr to the message that was sent.
     * @param error The error code resulting from the send operation.
     * @param sent_size The number of bytes sent.
     */
    void handle_recv(const boost::system::error_code& error,
                     std::size_t bytes_recvd);

    std::vector<uint8_t> get_metric_response(const std::string& metric_id);
    void handle_request(Request& request);

    void handle_sent(std::shared_ptr<std::vector<uint8_t>> response_enc,
                     const boost::system::error_code& error,
                     std::size_t bytes_transferred);
};