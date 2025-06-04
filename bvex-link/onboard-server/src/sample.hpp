#pragma once

#include "codec/primitive.hpp"
#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

typedef std::string MetricId;

struct SampleMetadata {
    MetricId metric_id; // Should be unique to each metric
    float timestamp;    // Time since last epoch
};

class SampleData
{
  public:
    const SampleMetadata metadata;
    const std::string type;
    SampleData(SampleMetadata metadata) : metadata(metadata) {}
    virtual ~SampleData() = default;
    virtual std::vector<uint8_t> encode_data() = 0;

    // returns nullptr if not implemented or error
    virtual std::vector<uint8_t> encode_response() = 0;
};

class PrimitiveSample : public SampleData
{
  public:
    const PrimitiveValue value;
    const std::string type = "primitive";
    PrimitiveSample(SampleMetadata metadata, PrimitiveValue value)
        : SampleData(metadata), value(value)
    {
    }
    std::vector<uint8_t> encode_data() override;
    std::vector<uint8_t> encode_response() override;
};

class FileSample : public SampleData
{
  public:
    const std::string file_path;
    const std::string file_extension;
    const std::string type = "file";
    FileSample(SampleMetadata metadata, std::string file_path,
               std::string file_extension)
        : SampleData(metadata), file_path(file_path),
          file_extension(file_extension)
    {
    }

    std::vector<uint8_t> encode_data() override;
    std::vector<uint8_t> encode_response() override;
};