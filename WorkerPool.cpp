#include "WorkerPool.h"

WorkerPool::WorkerPool(size_t numWorkers, SharedQueryQueue& queue, IDatabaseManager& db)
    : numWorkers_(numWorkers), queue_(queue), db_(db) {}

WorkerPool::~WorkerPool() {
    // Safety net: if the caller forgot to call shutdown(), don't leave
    // joinable threads dangling (which would terminate the program).
    if (started_.load() && !stopped_.load()) {
        shutdown();
    }
}

void WorkerPool::start() {
    started_ = true;
    workers_.reserve(numWorkers_);
    for (size_t i = 0; i < numWorkers_; ++i) {
        workers_.emplace_back(&WorkerPool::workerLoop, this);
    }
}

void WorkerPool::workerLoop() {
    Query query;
    // waitAndPop() blocks (no busy-polling) until either a query is ready
    // or the queue has been stopped and fully drained.
    while (queue_.waitAndPop(query)) {
        // A query popped here belongs to exactly this worker -- pop()
        // happens inside SharedQueryQueue's locked section, so no other
        // worker can ever also receive it. If shutdown() is called while
        // this execute() is in flight, this call still runs to completion
        // before the worker loops back and sees the "stop" state.
        db_.execute(query);
    }
}

void WorkerPool::shutdown() {
    // Tell the queue no more work is coming and wake any sleeping workers.
    // Queries already sitting in the queue are NOT discarded -- waitAndPop
    // keeps returning them until the queue is actually empty.
    queue_.requestStop();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    stopped_ = true;
}
