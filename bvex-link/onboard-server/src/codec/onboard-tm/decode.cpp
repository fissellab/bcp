#include "decode.hpp"
#include "pb_generated/sample.pb.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <pb_decode.h>
#include <string>
#include <vector>

// returning ptr to SampleData is necessary here because
// it allows us to use polymorphism and return classes
// derived from the abstract class SampleData
std::optional<Sample> decode_payload(std::vector<uint8_t> payload)
{
    Sample sample = Sample_init_zero;

    /* Create a stream that reads from the buffer. */
    pb_istream_t stream =
        pb_istream_from_buffer(payload.data(), payload.size());

    /* Now we are ready to decode the message. */
    bool success = pb_decode(&stream, Sample_fields, &sample);

    /* Check for errors... */
    if(success) {
        std::cout << "sample.metric_id: " << sample.metric_id << std::endl;
        return sample;
    } else {
        printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
        return std::nullopt;
    }
}