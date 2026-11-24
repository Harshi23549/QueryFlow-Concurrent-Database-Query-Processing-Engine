#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include "queryflow/database/IDatabaseManager.h"
#include "queryflow/queue/SharedQueryQueue.h"

namespace queryflow {

class WorkerPool {
public:
    using StartHandler =
        std::function<void(
            const Query&, std::size_t, std::chrono::steady_clock::time_point)>;
    using CompletionHandler = std::function<void(
        const Query&,
        std::size_t,
        const DatabaseResult&,
        const std::string&,
        std::chrono::steady_clock::time_point,
        std::chrono::steady_clock::time_point)>;

    WorkerPool(size_t numWorkers,
               SharedQueryQueue& queue,
               IDatabaseManager& db,
               StartHandler onStart = {},
               CompletionHandler onComplete = {});
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    ~WorkerPool();

    void start();
    void shutdown();
    size_t workerCount() const { return numWorkers_; }

private:
    void workerLoop();

    size_t numWorkers_;
    SharedQueryQueue& queue_;
    IDatabaseManager& db_;
    StartHandler onStart_;
    CompletionHandler onComplete_;
    std::vector<std::thread> workers_;
    std::atomic<bool> started_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<size_t> nextWorkerId_{0};
};

}  // namespace queryflow
