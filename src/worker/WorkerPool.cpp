#include "queryflow/worker/WorkerPool.h"

#include <stdexcept>
#include <utility>

namespace queryflow {

WorkerPool::WorkerPool(size_t numWorkers,
                       SharedQueryQueue& queue,
                       IDatabaseManager& db,
                       StartHandler onStart,
                       CompletionHandler onComplete)
    : numWorkers_(numWorkers),
      queue_(queue),
      db_(db),
      onStart_(std::move(onStart)),
      onComplete_(std::move(onComplete)) {
    if (numWorkers_ == 0) {
        throw std::invalid_argument("WorkerPool requires at least one worker");
    }
}

WorkerPool::~WorkerPool() {
    if (!stopped_.load()) {
        shutdown();
    }
}

void WorkerPool::start() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true) || stopped_.load()) {
        throw std::logic_error("WorkerPool can only be started once");
    }

    workers_.reserve(numWorkers_);
    try {
        for (size_t i = 0; i < numWorkers_; ++i) {
            workers_.emplace_back(&WorkerPool::workerLoop, this);
        }
    } catch (...) {
        shutdown();
        throw;
    }
}

void WorkerPool::workerLoop() {
    Query query;
    const size_t workerIndex = nextWorkerId_.fetch_add(1);
    while (queue_.waitAndPop(query)) {
        const auto startedAt = std::chrono::steady_clock::now();
        if (onStart_) {
            onStart_(query, workerIndex, startedAt);
        }

        DatabaseResult result;
        std::string error;
        try {
            result = db_.execute(query);
        } catch (const std::exception& exception) {
            error = exception.what();
        } catch (...) {
            error = "Database execution failed with an unknown error";
        }

        if (onComplete_) {
            onComplete_(
                query,
                workerIndex,
                result,
                error,
                startedAt,
                std::chrono::steady_clock::now());
        }
    }
}

void WorkerPool::shutdown() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true)) {
        return;
    }

    queue_.requestStop();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

}  // namespace queryflow
