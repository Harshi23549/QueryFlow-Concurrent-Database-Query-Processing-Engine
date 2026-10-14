#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <vector>

#include "queryflow/core/Query.h"
#include "queryflow/scheduler/Scheduler.h"

namespace queryflow {

class SharedQueryQueue {
public:
    explicit SharedQueryQueue(SchedulingPolicy policy = SchedulingPolicy::FCFS);
    SharedQueryQueue(const SharedQueryQueue&) = delete;
    SharedQueryQueue& operator=(const SharedQueryQueue&) = delete;

    bool push(Query query);
    bool waitAndPop(Query& outQuery);
    void requestStop();
    size_t size() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Query> queue_;
    Scheduler scheduler_;
    bool stopping_ = false;
};

}  // namespace queryflow
