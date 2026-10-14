#include "queryflow/queue/SharedQueryQueue.h"

#include <utility>

namespace queryflow {

void SharedQueryQueue::push(Query query) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        queue_.push(std::move(query));
    }
    cv_.notify_one();
}

bool SharedQueryQueue::waitAndPop(Query& outQuery) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || stopping_; });

    if (queue_.empty()) {
        return false;
    }

    outQuery = std::move(queue_.front());
    queue_.pop();
    return true;
}

void SharedQueryQueue::requestStop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
}

size_t SharedQueryQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

}  // namespace queryflow
