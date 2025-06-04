#include <nlohmann/json.hpp>
#include "sample_frame.hpp"
#include <cstdint>
#include <string>
#include <vector>

using json = nlohmann::json;

std::vector<uint8_t> encode_sample_frame(
    const SampleFrameData& sample_frame_data)
{
    json sample_frame;
    sample_frame["metric_id"] = sample_frame_data.metric_id;
    sample_frame["sample_id"] = sample_frame_data.sample_id;
    sample_frame["timestamp"] = sample_frame_data.timestamp;
    sample_frame["data_type"] = sample_frame_data.data_type;
    sample_frame["segment"]["num_segments"] = sample_frame_data.num_segments;
    sample_frame["segment"]["seqnum"] = sample_frame_data.seqnum;
    sample_frame["segment"]["data"] = *sample_frame_data.data;
    std::string json_sample_frame = sample_frame.dump();
    auto byte_data = std::vector<uint8_t>(json_sample_frame.begin(), json_sample_frame.end());
    return byte_data;
}