#pragma once

#include "../command.hpp"
#include "../sample.hpp"
#include "chunker.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <vector>

typedef uint32_t SampleId;

// Class to handle the segmented transmission of samples
class SampleTransmitter
{
  public:
    SampleTransmitter(
        std::function<std::shared_ptr<SampleData>()> get_new_sample,
        std::function<size_t()> get_max_pkt_size, MetricId metric_id);

    // Get the next payload to downlink
    std::optional<std::vector<uint8_t>> get_pkt();

    // Mark a sequence number as succesfully recieved
    void handle_ack(const std::vector<SeqNum>& seqnums,
                    SampleId sample_id);

    // // Mark the sample as succesfully recieved
    // void signal_sample_recieved(SampleId sample_id);

  private:
    bool set_new_sample();
    void increment_itr();
    unsigned int get_itr_val();

    std::function<std::shared_ptr<SampleData>()> get_new_sample_;
    std::function<size_t()> get_max_pkt_size_;
    SampleMetadata sample_metadata_;
    SampleId sample_id_;
    std::unique_ptr<Chunker> sample_chunker_;
    std::set<unsigned int> unacked_seqnums_;
    std::string data_type_;
    std::set<unsigned int>::iterator unacked_seqnums_itr_;
};