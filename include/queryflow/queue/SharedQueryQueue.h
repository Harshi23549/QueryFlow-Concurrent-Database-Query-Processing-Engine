#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

#include "queryflow/core/Query.h"

namespace queryflow {

class SharedQueryQueue {
public:
    SharedQueryQueue() = default;
    SharedQueryQueue(const SharedQueryQueue&) = delete;
    SharedQueryQueue& operator=(const SharedQueryQueue&) = delete;

    void push(Query query);
    bool waitAndPop(Query& outQuery);
    void requestStop();
    size_t size() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Query> queue_;
    bool stopping_ = false;
};

}  // namespace queryflow
