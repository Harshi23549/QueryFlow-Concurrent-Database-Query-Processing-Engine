#pragma once

#include <chrono>
#include <string>

namespace queryflow {

struct QueryMetrics {
    std::string queryId;
    std::chrono::steady_clock::time_point submittedAt;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point finishedAt;

    double waitingTimeMs() const;
    double executionTimeMs() const;
    double responseTimeMs() const;
};

}  // namespace queryflow
