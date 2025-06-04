#include "../primitive.hpp"
#include "pb_encode.h"
#include "pb_generated/response.pb.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <optional>
#include <vector>

std::vector<uint8_t> encode_response(const Response& response)
{
    std::vector<uint8_t> buffer(Response_size);
    pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    bool success = pb_encode(&stream, Response_fields, &response);
    assert(success);
    buffer.resize(stream.bytes_written);
    return buffer;
}

std::vector<uint8_t> encode_primitive_response(const std::string& metric_id,
                                               const PrimitiveValue& value)
{
    Response response = Response_init_zero;
    // metric id must be < max size
    strcpy(response.metric_id, metric_id.c_str());
    std::visit(
        [&response](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr(std::is_same_v<T, int32_t>) {
                response.primitive.which_value =
                    primitive_Primitive_int_val_tag;
                response.primitive.value.int_val = arg;
            } else if constexpr(std::is_same_v<T, int64_t>) {
                response.primitive.which_value =
                    primitive_Primitive_long_val_tag;
                response.primitive.value.long_val = arg;
            } else if constexpr(std::is_same_v<T, float>) {
                response.primitive.which_value =
                    primitive_Primitive_float_val_tag;
                response.primitive.value.float_val = arg;
            } else if constexpr(std::is_same_v<T, double>) {
                response.primitive.which_value =
                    primitive_Primitive_double_val_tag;
                response.primitive.value.double_val = arg;
            } else if constexpr(std::is_same_v<T, bool>) {
                response.primitive.which_value =
                    primitive_Primitive_bool_val_tag;
                response.primitive.value.bool_val = arg;
            } else if constexpr(std::is_same_v<T, std::string>) {
                response.primitive.which_value =
                    primitive_Primitive_string_val_tag;
                strcpy(response.primitive.value.string_val, arg.c_str());
            } else {
                throw std::runtime_error("Unknown/unimplemented response value type");
            }
        },
        value);
    response.has_primitive = true;
    return encode_response(response);
}

std::vector<uint8_t> encode_failure_response(const std::string& metric_id)
{
    Response response = Response_init_zero;
    strcpy(response.metric_id, metric_id.c_str());
    response.has_primitive = false;
    return encode_response(response);
}