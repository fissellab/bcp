#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @file request_sample.h
 *  @brief API to request sample data from the onboard server.
 *
 *  Example usage:
 *  @code
 *  #include "request_sample.h"
 *  #include "connected_udp_socket.h"
 *
 *  int main() {
 *      char* metric_id = "altitude";
 *      char* node = "localhost";
 *      char* port = "3000";
 *      Requester requester = make_requester(metric_id, node, port);
 *      if (requester.socket_fd < 0) {
 *          printf("Error creating requester\n");
 *          return 1;
 *      }
 *      RequestDoubleResult result = request_double(&requester);
 *      if (result.err) {
 *          printf("Request error\n");
 *          return 1;
 *      }
 *      printf("Response value: %f\n", result.value);
 *      return 0;
 *  }
 *  @endcode
 */

#include "../src/generated/nanopb/request.pb.h"
#include "../src/generated/nanopb/response.pb.h"

/**
 * @example gtest/request_sample.cpp
 */

typedef enum {
    REQUEST_STATUS_OK = 0,
    REQUEST_STATUS_SOCKET_CREATION_ERROR = -1,
    REQUEST_STATUS_ENCODING_ERROR = -2,
    REQUEST_STATUS_SEND_ERROR = -3,
    REQUEST_STATUS_RECV_ERROR = -4,
    REQUEST_STATUS_DECODING_ERROR = -5,
    REQUEST_STATUS_INVALID_RESPONSE_TYPE = -6
} request_status_t;

/**
 * @brief Structure to hold requester information.
 */
typedef struct {
    int socket_fd;         // Socket file descriptor
    const char* metric_id; // Metric ID
} Requester;

/**
 * @brief Structure to hold arguments for making a requester.
 */
typedef struct {
    const char* metric_id; // Metric ID
    const char* node;      // Node address
    const char* service;   // Service address
} MakeRequesterArgs;

/**
 * @brief Creates a requester. Used by functions like request_int to
 * specify endpoint and metric ID to request a value from.
 *
 * @param metric_id The metric ID to request.
 * @param node The node to connect to.
 * @param service The service to connect to.
 * @return Requester structure.
 *
 * This requester object is used instead of requesting a metric directly from a
 * socket file descriptor to make it difficult to request different metric ids
 * from the same socket.
 *
 * Example usage:
 * ```
 * Requester altitudeRequester = make_requester(
 *    "altitude", "localhost", "3000");
 * if (altitudeRequester.socket_fd < 0) {
 *    // handle error
 * } else {
 *    // use altitudeRequester
 * }
 * ```
 * @note Threading multiple requests to the same Requester is not recommended.
 */
Requester make_requester(const char* metric_id, const char* node,
                         const char* service);

/**
 * @brief Structure to hold the result of an integer request.
 */
typedef struct {
    request_status_t err; // Error code
    int value;            // Integer value
} RequestIntResult;

/**
 * @brief Sends requests an integer value sample from the metrid id of reqr
 * from the server reqr.socket_fd is connected to.
 *
 * @param reqr Requester structure.
 * @return RequestIntResult structure containing error code and integer value.
 *
 * Example usage:
 * ```
 * RequestIntResult result = request_int(&requester)
 * if (result.err) {
 *    // handle error
 * } else {
 *    // use result.value
 * }
 * ```
 */
RequestIntResult request_int(const Requester* reqr);

/**
 * @brief Structure to hold the result of a string request.
 */
typedef struct {
    request_status_t err; // Error code
    char* value;          // String value
} RequestStringResult;

/**
 * @brief Sends a request for a string value sample from the metric id of reqr
 * from the server reqr.socket_fd is connected to.
 *
 * @param reqr Requester structure.
 * @return RequestStringResult structure containing error code and string value.
 *
 * Example usage:
 * ```
 * RequestStringResult result = request_string(&requester)
 * if (result.err) {
 *    // handle error
 * } else {
 *    // use result.value
 * }
 * ```
 */
RequestStringResult request_string(const Requester* reqr);

/**
 * @brief Structure to hold the result of a float request.
 */
typedef struct {
    request_status_t err; // Error code
    double value;         // Float value
} RequestFloatResult;

/**
 * @brief Sends a request for a float value sample from the metric id of reqr
 * from the server reqr.socket_fd is connected to. This function is blocking
 * and will wait for the send operation to complete and for the response to be
 * received.
 *
 * @param reqr Requester structure.
 * @return Structure containing error code and float value.
 *
 * Example usage:
 * ```
 * RequestFloatResult result = request_float(&requester)
 * if (result.err) {
 *    // handle error
 * } else {
 *    // use result.value
 * }
 * ```
 */
RequestFloatResult request_float(const Requester* reqr);

/**
 * @brief Structure to hold the result of a double request.
 */
typedef struct {
    request_status_t err; // Error code
    double value;         // Double value
} RequestDoubleResult;

/**
 * @brief Sends a request for a double value sample from the metric id of reqr
 * from the server reqr.socket_fd is connected to. This function is blocking
 * and will wait for the send operation to complete and for the response to be
 * received.
 *
 * @param reqr Requester structure.
 * @return RequestDoubleResult structure containing error code and double value.
 *
 * Example usage:
 * ```
 * RequestDoubleResult result = request_double(&requester)
 * if (result.err) {
 *    // handle error
 * } else {
 *    // use result.value
 * }
 * ```
 */
RequestDoubleResult request_double(const Requester* reqr);

#ifdef __cplusplus
}
#endif
