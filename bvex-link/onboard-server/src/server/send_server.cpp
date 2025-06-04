#include "send_server.hpp"

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

// Lets us use s and ms after literal numbers
using namespace std::chrono_literals;

constexpr std::chrono::milliseconds MAX_WAIT_TIME = 1000ms;
constexpr std::chrono::milliseconds MIN_WAIT_TIME =
    1ms; // Set wait to <= 0 to disable backoff

// std::unique_ptr<std::string> get_message()
// {
//     return std::unique_ptr<std::string>(new std::string("Hello"));
// }

SendServer::SendServer(boost::asio::io_service& io_service, udp::socket& socket,
                       udp::endpoint& target_endpoint, Telemetry& telemetry,
                       Command& command)
    : socket_(socket), target_endpoint_(target_endpoint), telemetry_(telemetry),
      command_(command), schedule_send_timer_(io_service),
      backoff_timer_(io_service), current_wait_time_(MIN_WAIT_TIME)
{
    SendServer::start_send();
}

void SendServer::start_send()
{
    std::optional<std::vector<uint8_t>> message = telemetry_.pop();
    if(message) {
        auto message_ptr = std::make_shared<std::vector<uint8_t>>(*message);
        current_wait_time_ = MIN_WAIT_TIME; // reset exponential backoff

        // calls this.handle_send, giving
        // it the shared_ptr to the message
        // once the data is handed off to the OS
        // networking stack for transmission
        socket_.async_send_to(
            boost::asio::buffer(*message_ptr), target_endpoint_,
            boost::bind(&SendServer::handle_send, this, message_ptr,
                        boost::asio::placeholders::error,
                        boost::asio::placeholders::bytes_transferred));
    } else {
        // If we don't have a message to send, back off for some time
        // before trying again
        if(command_.metric_exists("test")) {
            std::cout << "test" << std::endl;
            // command_.get_metric_info("test")
            // std::cout << command_.g
        }
        // only back off if max wait is greater than 0
        if(MAX_WAIT_TIME > 0ms) {
#ifdef DEBUG
            // std::cout << "Telemetry.pop returned nullptr, waiting "
            //           << std::to_string(current_wait_time_.count()) << "ms"
            //           << std::endl;
#endif

            backoff_timer_.expires_after(current_wait_time_);
            backoff_timer_.async_wait(
                boost::bind(&SendServer::start_send, this));

            // Exponentially increase how much we back off. Cap under
            // max_wait_time_
            if((current_wait_time_ * 2) < MAX_WAIT_TIME) {
                current_wait_time_ *= 2;
            } else {
                current_wait_time_ = MAX_WAIT_TIME;
            }
        }
    }
}

// Handles
void SendServer::handle_send(std::shared_ptr<std::vector<uint8_t>> /*message*/,
                             const boost::system::error_code& error,
                             std::size_t sent_size /*bytes_transferred*/)
{
    if(error) {
        std::cerr << "Error code" << error.to_string()
                  << "on receive msg: " << error.message() << std::endl;
    }
    // Schedule sending of next packet, given the number of bytes sent
    SendServer::schedule_send(sent_size);
}

// At the point this is called, we have already waited for async_send_to_ to
// give the packet to the OS. Given the number of bytes sent in the last packet
// and our desired bps, calculate the time until the next packet
// can be sent assuming the OS sends the previous packet at
// our desired bps. Then, schedule the next packet to be sent
// at that time. Of course, the OS may have delays in sending the packet
// for whatever reason, so we may need to add add additional delay to
// ensure we don't lose the packet, given a hard cap of bps.
void SendServer::schedule_send(std::size_t sent_size)
{
    auto bits_sent = sent_size * 8;
    auto seconds_until_sent = bits_sent / command_.get_bps();
    auto interval = std::chrono::milliseconds(seconds_until_sent * 1000);
#ifdef DEBUG
    std::cout << "Sent " << sent_size << " bytes to port "
              << target_endpoint_.port() << std::endl;
    std::cout << "Waiting " << std::to_string(interval.count())
              << "ms to send next packet" << std::endl;
#endif
    // Set the timer's expiry time relative to now.
    schedule_send_timer_.expires_after(interval);
    schedule_send_timer_.async_wait(boost::bind(&SendServer::start_send, this));
}