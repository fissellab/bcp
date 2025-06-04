#pragma once

/** @file send_sample.h
 *  @brief API to send sample data to the onboard server.
 *
 *  Example usage:
 *  @code
 *  #include "send_sample.h"
 *  #include "connected_udp_socket.h"
 *
 *  #include <ctime>
 *  int main() {
 *      int socket_fd = connected_udp_socket(SAMPLE_SERVER_ADDR,
 *                                                 SAMPLE_SERVER_PORT);
 *      if (socket_fd < 0) {
 *          printf("Error connecting to server\n");
 *          return 1;
 *      }
 *      float current_time = time(NULL);
 *      send_sample_int32(socket_fd, "altitude", current_time, 50392);
 *      send_sample_float(socket_fd, "yaw", current_time, 0.1452f);
 *      close(socket_fd);
 *      return 0;
 *  }
 *  @endcode
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @example gtest/send_sample.cpp
 */

typedef enum {
    SEND_STATUS_OK = 0,
    SEND_STATUS_ENCODING_ERROR,
    SEND_STATUS_SEND_ERROR,
    SEND_STATUS_MEMORY_ALLOCATION_ERROR,
    SEND_STATUS_THREAD_CREATION_ERROR,
#ifdef BCP_FETCH_BOUNDS_CHECKING
    BOUNDS_CHECK_ERROR,
#endif
} send_status_t;

#define STRING_VALUE_MAX_SIZE 4096
#define METRIC_ID_MAX_SIZE 64
#define FILE_PATH_MAX_SIZE 128
#define EXTENSION_MAX_SIZE 16

/**
 * @brief Sends an int32_t sample.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param value The int32_t value to send.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_int32(int socket_fd, const char* metric_id,
                                float timestamp, int32_t value);

/**
 * @brief Sends an int64_t sample.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param value The int64_t value to send.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_int64(int socket_fd, const char* metric_id,
                                float timestamp, int64_t value);

/**
 * @brief Sends a float sample.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param value The float value to send.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_float(int socket_fd, const char* metric_id,
                                float timestamp, float value);

/**
 * @brief Sends a double sample.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param value The double value to send.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_double(int socket_fd, const char* metric_id,
                                 float timestamp, double value);

/**
 * @brief Sends a boolean sample.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param value The boolean value to send.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_bool(int socket_fd, const char* metric_id,
                               float timestamp, bool value);

/**
 * @brief Sends a string sample.
 *
 * @note The string value must have a null terminator.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param value The string value to send. Must be less than
 * STRING_VALUE_MAX_SIZE.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_string(int socket_fd, const char* metric_id,
                                 float timestamp, const char* value);

/**
 * @brief Sends a file sample.
 *
 * @param socket_fd The socket file descriptor.
 * @param metric_id The identifier for the metric. Must be less than
 * METRIC_ID_MAX_SIZE.
 * @param timestamp The timestamp of the sample.
 * @param filepath The path to the file to send. Must be less than
 * FILE_PATH_MAX_SIZE.
 * @param extension The file extension. Must be less than EXTENSION_MAX_SIZE.
 * @return send_status_t Status code indicating success or type of error.
 */
send_status_t send_sample_file(int socket_fd, const char* metric_id,
                               float timestamp, const char* filepath,
                               const char* extension);

#ifdef __cplusplus
}
#endif
