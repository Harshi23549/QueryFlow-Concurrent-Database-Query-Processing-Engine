#pragma once

#include <condition_variable>
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

    Query submit(const std::string& sql, int priority = 0);
    std::vector<QuerySnapshot> getQueries() const;
    std::optional<QuerySnapshot> getQuery(int id) const;
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

    SharedQueryQueue queue_;
    WorkerPool workerPool_;
    mutable std::mutex mutex_;
    std::condition_variable idleCondition_;
    std::map<int, QuerySnapshot> queries_;
    int nextId_ = 1;
    bool stopping_ = false;
};

}  // namespace queryflow
