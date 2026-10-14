#include "queryflow/queue/SharedQueryQueue.h"

#include <utility>

namespace queryflow {

SharedQueryQueue::SharedQueryQueue(SchedulingPolicy policy)
    : scheduler_(policy) {}

bool SharedQueryQueue::push(Query query) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return false;
        }
        queue_.push_back(std::move(query));
    }
    cv_.notify_one();
    return true;
}

bool SharedQueryQueue::waitAndPop(Query& outQuery) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty() || stopping_; });

    if (queue_.empty()) {
        return false;
    }

    const int index = scheduler_.selectNext(queue_);
    outQuery = std::move(queue_[static_cast<size_t>(index)]);
    queue_.erase(queue_.begin() + index);
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
