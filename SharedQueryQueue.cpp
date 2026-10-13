#include "SharedQueryQueue.h"

#include <utility>

void SharedQueryQueue::push(Query query) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(query));
    }
    // Notify outside the lock so the woken thread doesn't immediately block
    // again trying to re-acquire a mutex we're still holding.
    cv_.notify_one();
}

bool SharedQueryQueue::waitAndPop(Query& outQuery) {
    std::unique_lock<std::mutex> lock(mutex_);

    // Sleep until there's either work to do or we've been told to stop.
    // Using a predicate here also protects against spurious wakeups.
    cv_.wait(lock, [this] { return !queue_.empty() || stopping_; });

    if (queue_.empty()) {
        // Only reachable when stopping_ is true (per the predicate above)
        // and nothing is left to process -- genuinely time to exit.
        return false;
    }

    // pop() happens inside this same locked section as the emptiness check
    // above, so two workers can never observe and remove the same front
    // element: this is the whole basis for the "exactly once" guarantee.
    outQuery = std::move(queue_.front());
    queue_.pop();
    return true;
}

void SharedQueryQueue::requestStop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    // Every worker may currently be asleep in cv_.wait(); wake all of them
    // so each can re-check the predicate and either drain remaining work or
    // exit.
    cv_.notify_all();
}

size_t SharedQueryQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}
