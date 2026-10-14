#include "queryflow/worker/WorkerPool.h"

namespace queryflow {

WorkerPool::WorkerPool(size_t numWorkers, SharedQueryQueue& queue, IDatabaseManager& db)
    : numWorkers_(numWorkers), queue_(queue), db_(db) {}

WorkerPool::~WorkerPool() {
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
    while (queue_.waitAndPop(query)) {
        db_.execute(query);
    }
}

void WorkerPool::shutdown() {
    queue_.requestStop();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    stopped_ = true;
}

}  // namespace queryflow
