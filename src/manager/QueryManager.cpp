#include "queryflow/manager/QueryManager.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace queryflow {
namespace {

bool isBlank(const std::string& value) {
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
}

}  // namespace

QueryManager::QueryManager(std::size_t workerCount,
                           SchedulingPolicy policy,
                           IDatabaseManager& database)
    : queue_(policy),
      workerPool_(
          workerCount,
          queue_,
          database,
          [this](const Query& query, auto startedAt) {
              markStarted(query, startedAt);
          },
          [this](const Query& query,
                 const DatabaseResult& result,
                 const std::string& error,
                 auto startedAt,
                 auto finishedAt) {
              markCompleted(query, result, error, startedAt, finishedAt);
          }) {
    workerPool_.start();
}

QueryManager::~QueryManager() {
    shutdown();
}

Query QueryManager::submit(const std::string& sql, int priority) {
    if (isBlank(sql)) {
        throw std::invalid_argument("Query SQL cannot be empty");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
        throw std::logic_error("Cannot submit a query after shutdown");
    }

    Query query(nextId_++, sql, priority);
    QuerySnapshot snapshot;
    snapshot.id = query.id;
    snapshot.sql = query.sql;
    snapshot.priority = query.priority;
    snapshot.status = QueryStatus::Queued;
    snapshot.submitTime = query.submitTime;
    queries_.emplace(query.id, std::move(snapshot));

    if (!queue_.push(query)) {
        queries_.erase(query.id);
        throw std::logic_error("The query queue is shutting down");
    }
    return query;
}

std::vector<QuerySnapshot> QueryManager::getQueries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QuerySnapshot> snapshots;
    snapshots.reserve(queries_.size());
    for (const auto& entry : queries_) {
        snapshots.push_back(entry.second);
    }
    return snapshots;
}

std::optional<QuerySnapshot> QueryManager::getQuery(int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = queries_.find(id);
    if (found == queries_.end()) {
        return std::nullopt;
    }
    return found->second;
}

void QueryManager::waitUntilIdle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idleCondition_.wait(lock, [this] {
        return std::all_of(queries_.begin(), queries_.end(), [](const auto& entry) {
            return entry.second.status == QueryStatus::Completed ||
                   entry.second.status == QueryStatus::Failed;
        });
    });
}

void QueryManager::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }
    workerPool_.shutdown();
    idleCondition_.notify_all();
}

void QueryManager::markStarted(
    const Query& query, std::chrono::steady_clock::time_point startedAt) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = queries_.find(query.id);
    if (found == queries_.end()) {
        return;
    }
    found->second.status = QueryStatus::Running;
    found->second.startTime = startedAt;
}

void QueryManager::markCompleted(
    const Query& query,
    const DatabaseResult& result,
    const std::string& error,
    std::chrono::steady_clock::time_point startedAt,
    std::chrono::steady_clock::time_point finishedAt) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto found = queries_.find(query.id);
        if (found == queries_.end()) {
            return;
        }
        auto& snapshot = found->second;
        snapshot.status = error.empty() ? QueryStatus::Completed : QueryStatus::Failed;
        snapshot.startTime = startedAt;
        snapshot.finishTime = finishedAt;
        snapshot.result = result.message;
        snapshot.error = error;
    }
    idleCondition_.notify_all();
}

}  // namespace queryflow
