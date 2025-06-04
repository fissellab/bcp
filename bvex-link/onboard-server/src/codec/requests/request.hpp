#pragma once

#include "pb_generated/request.pb.h"
#include <cstdint>
#include <vector>
#include <optional>

std::optional<Request> decode_request(
    const std::vector<uint8_t>& payload);
