#include "queryflow/metrics/QueryMetrics.h"

namespace queryflow {

double QueryMetrics::waitingTimeMs() const {
    return std::chrono::duration<double, std::milli>(
        startedAt - submittedAt).count();
}

double QueryMetrics::executionTimeMs() const {
    return std::chrono::duration<double, std::milli>(
        finishedAt - startedAt).count();
}

double QueryMetrics::responseTimeMs() const {
    return std::chrono::duration<double, std::milli>(
        finishedAt - submittedAt).count();
}

}  // namespace queryflow
