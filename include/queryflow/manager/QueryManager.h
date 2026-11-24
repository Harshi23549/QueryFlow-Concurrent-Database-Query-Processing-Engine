#pragma once

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "queryflow/core/Query.h"
#include "queryflow/database/IDatabaseManager.h"
#include "queryflow/queue/SharedQueryQueue.h"
#include "queryflow/scheduler/Scheduler.h"
#include "queryflow/worker/WorkerPool.h"

namespace queryflow {

class QueryManager {
public:
    QueryManager(std::size_t workerCount,
                 SchedulingPolicy policy,
                 IDatabaseManager& database);
    QueryManager(const QueryManager&) = delete;
    QueryManager& operator=(const QueryManager&) = delete;
    ~QueryManager();

    Query submit(const std::string& sql,
                 int priority = 0,
                 const std::string& userId = {},
                 const std::string& sessionLabel = {},
                 const std::string& operation = "custom");
    Query submitPrepared(
        const std::string& sql,
        std::vector<std::optional<std::string>> parameters,
        int priority = 0,
        const std::string& userId = {},
        const std::string& sessionLabel = {},
        const std::string& operation = "custom");
    std::vector<QuerySnapshot> getQueries() const;
    std::vector<Query> getPendingQueries() const;
    std::optional<QuerySnapshot> getQuery(int id) const;
    std::vector<QueryEvent> getEvents() const;
    std::vector<WorkerSnapshot> getWorkers() const;
    std::size_t queueSize() const;
    SchedulingPolicy schedulingPolicy() const { return policy_; }
    std::size_t workerCount() const { return workerPool_.workerCount(); }
    void waitUntilIdle();
    void shutdown();

private:
    void markStarted(const Query& query,
                     std::size_t workerIndex,
                     std::chrono::steady_clock::time_point startedAt);
    void markCompleted(const Query& query,
                       std::size_t workerIndex,
                       const DatabaseResult& result,
                       const std::string& error,
                       std::chrono::steady_clock::time_point startedAt,
                       std::chrono::steady_clock::time_point finishedAt);
    void recordEventLocked(const QuerySnapshot& query,
                           const std::string& type,
                           const std::string& detail,
                           const std::string& workerId = {});

    SchedulingPolicy policy_;
    SharedQueryQueue queue_;
    WorkerPool workerPool_;
    mutable std::mutex mutex_;
    std::condition_variable idleCondition_;
    std::map<int, QuerySnapshot> queries_;
    std::vector<QueryEvent> events_;
    std::vector<WorkerSnapshot> workers_;
    int nextId_ = 1;
    std::uint64_t nextArrivalSequence_ = 1;
    std::uint64_t nextEventId_ = 1;
    bool stopping_ = false;
};

}  // namespace queryflow
