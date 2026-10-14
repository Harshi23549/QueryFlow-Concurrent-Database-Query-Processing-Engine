#pragma once

#include <chrono>
#include <vector>

#include "queryflow/core/Query.h"

namespace queryflow {

enum class SchedulingPolicy {
    FCFS,
    PRIORITY
};

class Scheduler {
public:
    explicit Scheduler(SchedulingPolicy policy);

    void setPolicy(SchedulingPolicy policy);
    SchedulingPolicy policy() const { return policy_; }
    int selectNext(const std::vector<Query>& queries) const;

private:
    SchedulingPolicy policy_;
};

}  // namespace queryflow
