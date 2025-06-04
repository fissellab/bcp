#include "request.hpp"
#include "pb_generated/request.pb.h"
#include <cstdint>
#include <iostream>
#include <pb_decode.h>
#include <string>
#include <vector>
#include <optional>

std::optional<Request> decode_request(
    const std::vector<uint8_t>& payload)
{
    Request request = Request_init_zero;
    bool status;

    /* Create a stream that reads from the buffer. */
    pb_istream_t stream =
        pb_istream_from_buffer(payload.data(), payload.size());

    /* Now we are ready to decode the message. */
    status = pb_decode(&stream, Request_fields, &request);

    /* Check for errors... */
    if(!status) {
        printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
        return std::nullopt;
    }

    return request;
}