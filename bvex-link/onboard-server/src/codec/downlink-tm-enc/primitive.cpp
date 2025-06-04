#include "primitive.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

using json = nlohmann::json;

std::vector<uint8_t> encode_primitive(
    const PrimitiveValue& value)
{
    json primitive_frame;
    std::visit(
        [&primitive_frame](auto&& arg) { primitive_frame["value"] = arg; },
        value);
    std::string primitive_frame_string = primitive_frame.dump();
    return std::vector<uint8_t>(
        primitive_frame_string.begin(), primitive_frame_string.end());
}