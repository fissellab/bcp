#pragma once

#include "pb_generated/sample.pb.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>

std::optional<Sample> decode_payload(
    std::vector<uint8_t> payload);
