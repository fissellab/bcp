#include "onboard_telemetry_recv_server.hpp"
#include "recv_server.hpp"
#include <codec/onboard-tm/decode.hpp>
#include <codec/onboard-tm/pb_generated/sample.pb.h>
#include <iostream>
#include <memory>

OnboardTelemetryRecvServer::OnboardTelemetryRecvServer(
    udp::socket& listen_socket, Command& command, std::size_t buffer_size)
    : recv_server_(listen_socket,
                   std::bind(&OnboardTelemetryRecvServer::handle_message, this,
                             std::placeholders::_1),
                   buffer_size),
      command_(command) {};

std::unique_ptr<SampleData> sample_struct_to_sample_data(Sample sample)
{
    std::string metric_id(sample.metric_id);
    float timestamp = sample.timestamp;
    SampleMetadata metadata = {.metric_id = metric_id, .timestamp = timestamp};

    std::unique_ptr<SampleData> sample_data;
#ifdef DEBUG_ONBOARD_RECV_SERVER
    std::cout << "sample.which_data: " << sample.which_data << std::endl;
#endif
    switch(sample.which_data) {
    case Sample_primitive_tag:
#ifdef DEBUG_ONBOARD_RECV_SERVER
        std::cout << "data type: Primitive" << std::endl;
#endif
        switch(sample.data.primitive.which_value) {
        case primitive_Primitive_int_val_tag:

#ifdef DEBUG_ONBOARD_RECV_SERVER
            std::cout << "primitive type: int" << std::endl;
#endif
            return std::make_unique<PrimitiveSample>(
                metadata, sample.data.primitive.value.int_val);
        case primitive_Primitive_float_val_tag:
#ifdef DEBUG_ONBOARD_RECV_SERVER
            std::cout << "primitive type: float" << std::endl;
#endif
            return std::make_unique<PrimitiveSample>(
                metadata, sample.data.primitive.value.float_val);
        case primitive_Primitive_double_val_tag:
#ifdef DEBUG_ONBOARD_RECV_SERVER
            std::cout << "primitive type: double" << std::endl;
#endif
            return std::make_unique<PrimitiveSample>(
                metadata, sample.data.primitive.value.double_val);
        case primitive_Primitive_string_val_tag:
#ifdef DEBUG_ONBOARD_RECV_SERVER
            std::cout << "primitive type: string" << std::endl;
#endif
            return std::make_unique<PrimitiveSample>(
                metadata, sample.data.primitive.value.string_val);
        default:
            std::cerr << "Unknown/unimplemented value type" << std::endl;
            return nullptr;
        }
    case Sample_file_tag:
#ifdef DEBUG_ONBOARD_RECV_SERVER
        std::cout << "data type: File" << std::endl;
#endif
        return std::make_unique<FileSample>(metadata, sample.data.file.filepath,
                                            sample.data.file.extension);
    default:
        std::cerr << "Unknown/unimplemented data type" << std::endl;
        return nullptr;
    }
}

void OnboardTelemetryRecvServer::handle_message(
    std::unique_ptr<std::vector<uint8_t>> message)
{
#ifdef DEBUG_ONBOARD_RECV_SERVER
    std::cout << "Received onboard telemetry message of " << message->size()
              << " bytes." << std::endl;
#endif
    std::optional<Sample> sample = decode_payload(*message);
    if(sample) {
#ifdef DEBUG_ONBOARD_RECV_SERVER
        std::cout << "Message decoded to Sample succesfully." << std::endl;
#endif
        command_.add_sample(sample_struct_to_sample_data(*sample));
    } else {
#ifndef DEBUG
        std::cout << "Decoding message to Sample failed." << std::endl;
#endif
    }
}
