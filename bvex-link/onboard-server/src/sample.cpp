#include "sample.hpp"
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <codec/downlink-tm-enc/file.hpp>
#include <codec/downlink-tm-enc/primitive.hpp>
#include <codec/requests/response.hpp>

#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

std::vector<uint8_t> PrimitiveSample::encode_data()
{
    return encode_primitive(value);
};

std::vector<uint8_t> PrimitiveSample::encode_response()
{
    return encode_primitive_response(metadata.metric_id, value);
}

std::vector<uint8_t> FileSample::encode_data()
{
    return encode_file(file_path, file_extension);
};

std::vector<uint8_t> FileSample::encode_response()
{
    return encode_failure_response(metadata.metric_id);
};