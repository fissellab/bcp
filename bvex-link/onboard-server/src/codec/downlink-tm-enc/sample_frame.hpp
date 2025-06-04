#pragma once

#include <boost/shared_ptr.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct SampleFrameData {
    std::string metric_id;
    float timestamp;
    std::string data_type;
    unsigned int sample_id;
    unsigned int num_segments;
    unsigned int seqnum;
    std::unique_ptr<std::vector<uint8_t>> data;
};

std::vector<uint8_t> encode_sample_frame(const SampleFrameData& sample_frame_data);