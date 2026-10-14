#include "queryflow/scheduler/Scheduler.h"

namespace queryflow {

Scheduler::Scheduler(SchedulingPolicy policy)
    : policy_(policy) {}

void Scheduler::setPolicy(SchedulingPolicy policy) {
    policy_ = policy;
}

int Scheduler::selectNext(const std::vector<Query>& queries) const {
    if (queries.empty()) {
        return -1;
    }

    int best = 0;
    for (int i = 1; i < static_cast<int>(queries.size()); ++i) {
        bool chooseCurrent = false;

        if (policy_ == SchedulingPolicy::FCFS) {
            chooseCurrent = queries[i].submitTime < queries[best].submitTime;
        } else {
            if (queries[i].priority > queries[best].priority) {
                chooseCurrent = true;
            } else if (queries[i].priority == queries[best].priority &&
                       queries[i].submitTime < queries[best].submitTime) {
                chooseCurrent = true;
            }
        }

        if (chooseCurrent) {
            best = i;
        }
    }

    return best;
}

}  // namespace queryflow
