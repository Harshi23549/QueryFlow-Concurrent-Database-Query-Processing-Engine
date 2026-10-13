#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

#include "Query.h"

// Thread-safe FIFO queue connecting the Query Manager (producer) to the
// worker pool (consumers).
//
//   push()       -- producer side, called from any thread.
//   waitAndPop() -- consumer side, called from worker threads. Blocks the
//                   calling thread (no busy-polling) until either work is
//                   available or the queue has been stopped and drained.
//
// A single std::mutex guards the underlying std::queue, and a
// std::condition_variable lets idle workers sleep instead of spinning.
class SharedQueryQueue {
public:
    SharedQueryQueue() = default;

    // Non-copyable, non-movable: workers hold a reference to one instance.
    SharedQueryQueue(const SharedQueryQueue&) = delete;
    SharedQueryQueue& operator=(const SharedQueryQueue&) = delete;

    // Producer: enqueue a query and wake one sleeping worker.
    void push(Query query);

    // Consumer: block until a query is available or the queue is stopped
    // and empty. Returns true and fills `outQuery` if a query was popped;
    // returns false to signal "no more work, time to exit".
    bool waitAndPop(Query& outQuery);

    // Wake every worker so they can re-check their exit condition. This does
    // NOT discard queued work: waitAndPop() keeps returning true for
    // whatever is still sitting in the queue. It only starts returning false
    // once the queue is both stopped *and* empty, so a shutdown mid-burst
    // still lets every already-pushed query get executed.
    void requestStop();

    size_t size() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Query> queue_;
    bool stopping_ = false;
};
