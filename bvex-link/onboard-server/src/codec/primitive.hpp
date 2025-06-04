#pragma once

#include <variant>
#include <cstdint>
#include <string>

// primitive values supported to be encoded
typedef std::variant<int32_t, int64_t, float, double, bool, std::string>
    PrimitiveValue;