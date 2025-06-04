#include "telemetry.hpp"
#include "command.hpp"
#include "sample.hpp"
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

Telemetry::Telemetry(Command& command)
    : command_(command), metric_iter_(command.get_metric_iterator()) {};

std::optional<std::vector<uint8_t>> Telemetry::pop(unsigned int retry_depth)
{
    std::optional<SampleInfo> sample = metric_iter_.get_next_metric_sample();
    if (!sample.has_value()) {
        return std::nullopt;
    }

    std::string metric_id = sample->metric_id;

    // if metric id not in metric token counts, add it with initial value
    // of 1
    if(metric_token_counts_.find(metric_id) ==
       metric_token_counts_.end()) {
        metric_token_counts_[metric_id] = 1;
    }

    int& tokens = metric_token_counts_[metric_id];
    int threshold = sample->token_threshold;
    metric_token_counts_[metric_id]++;
    if(tokens >= threshold) {
        auto payload = sample->get_pkt();
        if(payload) {
            tokens = 0;
            return payload;
        }
    }
    tokens++;
    if(retry_depth < command_.get_num_metrics()) {
        return pop(retry_depth + 1);
    } else {
        return std::nullopt;
    }
};