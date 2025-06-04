#include "../primitive.hpp"
#include "pb_encode.h"
#include "pb_generated/response.pb.h"
#include <cstdint>
#include <vector>

std::vector<uint8_t> encode_response(const Response& response);

std::vector<uint8_t> encode_primitive_response(const std::string& metric_id,
                                               const PrimitiveValue& value);

std::vector<uint8_t> encode_failure_response(const std::string& metric_id);
