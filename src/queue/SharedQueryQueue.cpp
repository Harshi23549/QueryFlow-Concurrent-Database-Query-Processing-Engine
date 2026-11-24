#include "queryflow/queue/SharedQueryQueue.h"

#include <algorithm>
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
        if (query.arrivalSequence == 0) {
            query.arrivalSequence = nextArrivalSequence_++;
        } else {
            nextArrivalSequence_ =
                std::max(nextArrivalSequence_, query.arrivalSequence + 1);
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

std::vector<Query> SharedQueryQueue::orderedQueries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto pending = queue_;
    std::vector<Query> ordered;
    ordered.reserve(pending.size());
    while (!pending.empty()) {
        const auto index = static_cast<std::size_t>(scheduler_.selectNext(pending));
        ordered.push_back(std::move(pending[index]));
        pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return ordered;
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
