#pragma once

#include "../command.hpp"
#include "../telemetry.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <string>

using boost::asio::ip::udp;

/*!
   Send loop:
   \dot
   digraph send_loop {
        SendServer -> start_send;

        start_send -> handle_send;
        start_send -> socket_;
        start_send -> io_service_;
        start_send -> recv_buffer_;

        handle_send -> schedule_send;

        schedule_send -> start_send;
        schedule_send -> timer_;
        schedule_send -> command_;
   }
   \enddot
 */

/**
 * @brief Sends telemetry data packets to a remote endpoint.
 *
 * Gets telemetry packets from the telemetry object and sends them
 * asynchronously from localhost:source_port to target_address:target_port using
 * the provided io_service. The send rate is capped by the command object's max
 * bps.
 */
class SendServer
{
  public:
    /**
     * @brief Construct a new SendServer object
     *
     * @param io_service The io_service to use for async network IO operations.
     * @param telemetry The telemetry object to get data from.
     * @param command The command object to get the bps from.
     * @param source_port The port to send from.
     * @param target_address The address to send to.
     * @param target_port The port to send to.
     */
    SendServer(boost::asio::io_service& io_service, udp::socket& socket,
               udp::endpoint& target_endpoint, Telemetry& telemetry,
               Command& command);

  private:
    udp::socket& socket_;
    udp::endpoint& target_endpoint_;
    Telemetry& telemetry_;
    Command& command_;
    std::vector<char> recv_buffer_;
    boost::asio::steady_timer schedule_send_timer_;
    boost::asio::steady_timer backoff_timer_;
    std::chrono::milliseconds
        current_wait_time_; /* time to wait in ms if telemetry.pop() gives no
                               result*/

    /**
     * @brief Sends the next telemetry data packet async, then calls
     * handle_send() on completion.
     *
     * Calls telemetry_.pop() to get the next telemetry data packet, then sends
     * it async to remote_endpoint_. Gives handle_send the shared_ptr to the
     * message, the boost error code, and the number of bytes sent.
     *
     */
    void start_send();

    /**
     * @brief Handles the completion of the async packet send called by
     * start_send.
     *
     * Reads out errors if there were any, then calls schedule_send().
     *
     * @param message Shared ptr to the message that was sent.
     * @param error The error code resulting from the send operation.
     * @param sent_size The number of bytes sent.
     */
    void handle_send(std::shared_ptr<std::vector<uint8_t>> message,
                     const boost::system::error_code& error,
                     std::size_t sent_size);

    /**
     * @brief Schedules the next call of self.start_send() to happen after OS
     * finishes sending previous packet.
     *
     * Calculates send time based on the number of bytes sent in the previous
     * packet and command_.get_bps().
     *
     * @param sent_size The number of bytes sent in the previous packet.
     */
    void schedule_send(std::size_t sent_size);
};