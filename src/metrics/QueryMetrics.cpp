#include "queryflow/metrics/QueryMetrics.h"

namespace queryflow {

QueryMetrics QueryMetrics::fromSnapshot(const QuerySnapshot& query) {
    return {std::to_string(query.id),
            query.submitTime,
            query.startTime,
            query.finishTime};
}

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
