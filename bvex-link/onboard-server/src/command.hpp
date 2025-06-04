#pragma once

#include "sample.hpp"
#include "utils/sample_transmitter.hpp"
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class SampleTransmitter;

struct Ack {
    MetricId metric_id;
    uint32_t sample_id;
    std::vector<uint32_t> seqnums;
};

/**
 * @brief Struct to hold information about a metric.
 */
struct MetricInfo {
    // Unique of the metric
    MetricId metric_id;
    // Amount of bps dedicated to metric telemetry data
    // Value in range [0-1]
    unsigned int token_threshold;
    // Latest sample of this metric recieved.
    // can be null if no sample has been recieved or if nullified after
    // pop_new_sample
    std::shared_ptr<SampleData> latest_sample;

    // whether the latest_sample has already been downlinked
    bool latest_downlinked;

    std::unique_ptr<SampleTransmitter> sample_transmitter;
};

struct SampleInfo {
    MetricId metric_id;
    unsigned int token_threshold;
    std::function<std::optional<std::vector<uint8_t>>()> get_pkt;
};

class MetricIterator
{
  public:
    MetricIterator(std::function<std::map<MetricId, MetricInfo>::iterator()>
                       get_begin_iterator,
                   std::function<std::map<MetricId, MetricInfo>::iterator()>
                       get_end_iterator);

    std::optional<SampleInfo> get_next_metric_sample();

  private:
    bool metrics_empty();
    bool is_iterator_valid();

    std::function<std::map<MetricId, MetricInfo>::iterator()>
        get_begin_iterator;
    std::function<std::map<MetricId, MetricInfo>::iterator()> get_end_iterator;
    std::map<MetricId, MetricInfo>::iterator current_iterator_;
};

class Command
{
  public:
    Command(size_t init_bps, size_t init_max_packet_size);
    // void add_tc_json(const std::string& telecommands_json);

    void handle_ack(Ack ack);

    size_t get_bps();

    void set_bps(size_t bps);

    /**
     * @brief Adds a sample to the internal data structure.
     * @param sample Shared pointer to the sample data to be added.
     */
    void add_sample(std::unique_ptr<SampleData> sample);

    /**
     * @brief Get the latest sample data recieved for the given
     * metric id. Returns std::nullopt if the metric does not exist
     */
    std::optional<std::vector<uint8_t>> get_latest_sample_response(
        MetricId metric_id);

    size_t get_num_metrics();

    void print_all_metric_ids();

    bool metric_exists(MetricId metric_id);

    size_t get_max_packet_size();

    void set_max_packet_size(size_t max_packet_size);

    MetricIterator get_metric_iterator();

  private:
    /**
     * @brief Get the latest sample data recieved for the given
     * metric ID if it has not already been downlinked and mark it as
     * downlinked. Returns nullptr if the latest sample has already been
     * downlinked.
     *
     * @param metric_id ID of the metric.
     * @return SampleData if available.
     */
    std::shared_ptr<SampleData> get_new_sample(MetricId metric_id);

    size_t bps_;
    size_t max_packet_size_;
    // std::vector<std::string> telecommands_;

    /**
     * @brief Stores metric_id:metric_info* pairs.
     *
     * std::map is used here because it can be iterated through in order,
     * and added to without iterator invalidation.
     *
     * @see https://en.cppreference.com/w/cpp/container#Iterator_invalidation
     */
    std::map<MetricId, MetricInfo> metrics_;
};
