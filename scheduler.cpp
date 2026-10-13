#include "scheduler.h"

#include <stdexcept>

Scheduler::Scheduler(SchedulingPolicy policy)
    : policy_(policy) {
}

void Scheduler::setPolicy(SchedulingPolicy policy) {
    policy_ = policy;
}

int Scheduler::selectNext(
    const std::vector<QueryTask>& queries) const {

    if (queries.empty()) {
        return -1;
    }

    int best = 0;

    for (int i = 1; i < static_cast<int>(queries.size()); ++i) {
        bool chooseCurrent = false;

        if (policy_ == SchedulingPolicy::FCFS) {
            chooseCurrent =
                queries[i].submittedAt < queries[best].submittedAt;
        } else {
            if (queries[i].priority > queries[best].priority) {
                chooseCurrent = true;
            } else if (queries[i].priority == queries[best].priority &&
                       queries[i].submittedAt <
                           queries[best].submittedAt) {
                chooseCurrent = true;
            }
        }

        if (chooseCurrent) {
            best = i;
        }
    }

    return best;
}

double Scheduler::waitingTimeMs(const QueryTask& query) {
    return std::chrono::duration<double, std::milli>(
        query.startedAt - query.submittedAt).count();
}

double Scheduler::executionTimeMs(const QueryTask& query) {
    return std::chrono::duration<double, std::milli>(
        query.finishedAt - query.startedAt).count();
}

double Scheduler::responseTimeMs(const QueryTask& query) {
    return std::chrono::duration<double, std::milli>(
        query.finishedAt - query.submittedAt).count();
}