#pragma once

#include <cstdint>
#include <vector>
#include <string>

std::vector<uint8_t> encode_file(const std::string& file_path,
                                 const std::string& extension);