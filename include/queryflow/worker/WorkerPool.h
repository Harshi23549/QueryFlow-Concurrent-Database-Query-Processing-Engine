#pragma once

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "queryflow/database/IDatabaseManager.h"
#include "queryflow/queue/SharedQueryQueue.h"

namespace queryflow {

class WorkerPool {
public:
    WorkerPool(size_t numWorkers, SharedQueryQueue& queue, IDatabaseManager& db);
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
    std::vector<std::thread> workers_;
    std::atomic<bool> started_{false};
    std::atomic<bool> stopped_{false};
};

}  // namespace queryflow
