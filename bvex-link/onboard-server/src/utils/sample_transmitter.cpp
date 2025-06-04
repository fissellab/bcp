#include "sample_transmitter.hpp"
#include "chunker.hpp"
#include <boost/iterator/counting_iterator.hpp>
#include <codec/downlink-tm-enc/sample_frame.hpp>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#define IPV4_OVERHEAD 20
#define UDP_OVERHEAD 20

SampleTransmitter::SampleTransmitter(
    std::function<std::shared_ptr<SampleData>()> get_new_sample,
    std::function<size_t()> get_max_pkt_size, MetricId metric_id)
    : get_new_sample_(get_new_sample), get_max_pkt_size_(get_max_pkt_size),
      sample_metadata_({
          .metric_id = metric_id,
          .timestamp = 0.0f,
      }),
      sample_id_(0), sample_chunker_(nullptr), unacked_seqnums_() {};

bool SampleTransmitter::set_new_sample()
{
    std::shared_ptr<SampleData> sample = get_new_sample_();
    if(sample == nullptr) {
        return false;
    } else {
        size_t overhead = IPV4_OVERHEAD + UDP_OVERHEAD;

        size_t max_segment_size = get_max_pkt_size_() - overhead;

        data_type_ = sample->type;
        sample_metadata_ = sample->metadata;

        sample_chunker_ = std::make_unique<Chunker>(
            Chunker(std::move(sample->encode_data()), max_segment_size));
        unsigned int num_chunks = sample_chunker_->get_num_chunks();

        // set all seqnums to unacked
        unacked_seqnums_.clear();
        std::copy(boost::counting_iterator<int>(0),
                  boost::counting_iterator<int>(num_chunks),
                  std::inserter(unacked_seqnums_, unacked_seqnums_.end()));

        // set iterator to first unacked seqnum
        unacked_seqnums_itr_ = unacked_seqnums_.begin();

        // increment to next sample id
        sample_id_++;

        return true;
    }
}

std::optional<std::vector<uint8_t>> SampleTransmitter::get_pkt()
{
    if(sample_chunker_ == nullptr || unacked_seqnums_.size() == 0) {
        // Get a new sample to downlink
        bool got_new_sample = set_new_sample();
        if(!got_new_sample) {
            return std::nullopt;
        }
    }
    unsigned int seq_num = get_itr_val();
    Chunk chunk = sample_chunker_->get_chunk(seq_num);
    increment_itr();
    SampleFrameData segment_data = {.metric_id = sample_metadata_.metric_id,
                                    .timestamp = sample_metadata_.timestamp,
                                    .data_type = data_type_,
                                    .sample_id = sample_id_,
                                    .num_segments =
                                        sample_chunker_->get_num_chunks(),
                                    .seqnum = seq_num,
                                    .data = std::move(chunk.data)};
    std::vector<uint8_t> pkt = encode_sample_frame(std::move(segment_data));
#ifdef DEBUG
    if(pkt.size() > get_max_pkt_size_()) {
        std::cerr << "Packet size exceeds maximum packet size" << std::endl;
        std::cerr << "Actual packet size: " << pkt.size() << std::endl;
        std::cerr << "Maximum packet size: " << get_max_pkt_size_()
                  << std::endl;
    }
#endif
    return pkt;
}
void SampleTransmitter::handle_ack(const std::vector<uint32_t>& seqnums,
                                   SampleId sample_id)
{
    if(sample_id != sample_id_) {
        return;
    }
    // TODO: Reposition itr dynamically as erasing
    // so we dont go back to start every erasure
    int num_erased = 0;
    for(auto seqnum : seqnums) {
        if(unacked_seqnums_.find(seqnum) != unacked_seqnums_.end()) {
            unacked_seqnums_.erase(seqnum);
            num_erased++;
        }
    }
    if(num_erased > 0) {
        unacked_seqnums_itr_ = unacked_seqnums_.begin();
    }
}

void SampleTransmitter::increment_itr()
{
    unacked_seqnums_itr_++;
    if(unacked_seqnums_itr_ == unacked_seqnums_.end()) {
        unacked_seqnums_itr_ = unacked_seqnums_.begin();
    }
}

unsigned int SampleTransmitter::get_itr_val() { return *unacked_seqnums_itr_; }
