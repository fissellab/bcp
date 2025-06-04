#pragma once

#include "command.hpp"
#include "sample.hpp"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <optional>

class Telemetry
{
  public:
    Telemetry(Command& command);

    /**
     * @brief Retrieves the next telemetry packet.
     *
     * The max size of the
     * data is limited to this.command_.get_max_packet_size()
     *
     * @return ptr to raw bytes of sample frame packet.
     */
    std::optional<std::vector<uint8_t>> pop(unsigned int retry_depth = 0);

  private:
    Command& command_;
    std::map<MetricId, int> metric_token_counts_;
    MetricIterator metric_iter_;
};
