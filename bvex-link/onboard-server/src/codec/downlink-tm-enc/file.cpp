#include "file.hpp"
#include <cstdint>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

std::vector<uint8_t> encode_file(const std::string& file_path,
                                 const std::string& extension)
{
    json file_frame;
    file_frame["extension"] = extension;

    std::vector<uint8_t> file_data;

    std::ifstream file(file_path, std::ios::binary);
    if(file) {
        file.unsetf(std::ios::skipws);
        file_data.insert(file_data.begin(),
                         std::istream_iterator<uint8_t>(file),
                         std::istream_iterator<uint8_t>());
    }
    file_frame["data"] = file_data;
    std::string file_frame_string = file_frame.dump();
    return std::vector<uint8_t>(file_frame_string.begin(),
                                file_frame_string.end());
}