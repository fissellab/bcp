#pragma once

#include "../primitive.hpp"
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Encodes a primitive value into protocol buffers "Primitive" message
 * encoded bytes
 *
 * Primitive is defined in primitive.proto in the shared directory.
 *
 * @param value The PrimitiveValue to encode.
 * @return A shared pointer to a vector of uint8_t containing the encoded
 * message.
 */
std::vector<uint8_t> encode_primitive(const PrimitiveValue& value);
