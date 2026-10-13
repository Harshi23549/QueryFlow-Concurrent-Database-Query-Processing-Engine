#pragma once

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "IDatabaseManager.h"
#include "SharedQueryQueue.h"

// Owns W1..WN worker threads. Each worker repeatedly calls
// SharedQueryQueue::waitAndPop() and, for every query it gets, calls
// IDatabaseManager::execute() on it.
//
//   start()    -- launches numWorkers worker threads.
//   shutdown() -- stops accepting new work, lets every already-queued query
//                 finish executing, then joins every thread. Safe to call
//                 once; the destructor calls it too as a safety net.
class WorkerPool {
public:
    WorkerPool(size_t numWorkers, SharedQueryQueue& queue, IDatabaseManager& db);

    // Not copyable: owns live threads.
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    ~WorkerPool();

    // Launch the worker threads. Safe to call only once per instance.
    void start();

    // Signal the shared queue to stop, then join every worker thread.
    // Blocks until every query that was pushed before (or during) this call
    // has been popped and executed -- nothing queued is ever dropped.
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
