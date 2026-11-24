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
        : policy_(policy),
            queue_(policy),
      workerPool_(
          workerCount,
          queue_,
          database,
          [this](const Query& query, auto workerIndex, auto startedAt) {
              markStarted(query, workerIndex, startedAt);
          },
          [this](const Query& query,
                 auto workerIndex,
                 const DatabaseResult& result,
                 const std::string& error,
                 auto startedAt,
                 auto finishedAt) {
              markCompleted(
                  query, workerIndex, result, error, startedAt, finishedAt);
          }) {
    workers_.reserve(workerCount);
    for (std::size_t index = 0; index < workerCount; ++index) {
        WorkerSnapshot worker;
        worker.workerId = "worker-" + std::to_string(index + 1);
        workers_.push_back(std::move(worker));
    }
    workerPool_.start();
}

QueryManager::~QueryManager() {
    shutdown();
}

Query QueryManager::submit(const std::string& sql,
                           int priority,
                           const std::string& userId,
                           const std::string& sessionLabel,
                           const std::string& operation) {
    return submitPrepared(sql, {}, priority, userId, sessionLabel, operation);
}

Query QueryManager::submitPrepared(
    const std::string& sql,
    std::vector<std::optional<std::string>> parameters,
    int priority,
    const std::string& userId,
    const std::string& sessionLabel,
    const std::string& operation) {
    if (isBlank(sql)) {
        throw std::invalid_argument("Query SQL cannot be empty");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (stopping_) {
        throw std::logic_error("Cannot submit a query after shutdown");
    }

    Query query(nextId_++, sql, priority);
    query.parameters = std::move(parameters);
    query.arrivalSequence = nextArrivalSequence_++;
    query.userId = userId;
    query.sessionLabel = sessionLabel;
    query.operation = operation;
    QuerySnapshot snapshot;
    snapshot.id = query.id;
    snapshot.arrivalSequence = query.arrivalSequence;
    snapshot.sql = query.sql;
    snapshot.userId = query.userId;
    snapshot.sessionLabel = query.sessionLabel;
    snapshot.operation = query.operation;
    snapshot.priority = query.priority;
    snapshot.status = QueryStatus::Queued;
    snapshot.submitTime = query.submitTime;
    snapshot.submittedAt = query.submittedAt;
    queries_.emplace(query.id, std::move(snapshot));

    if (!queue_.push(query)) {
        queries_.erase(query.id);
        throw std::logic_error("The query queue is shutting down");
    }
    const auto& storedSnapshot = queries_.at(query.id);
    recordEventLocked(storedSnapshot,
                      "query.received",
                      "Query accepted by QueryManager");
    recordEventLocked(storedSnapshot,
                      "query.id_assigned",
                      "Assigned query ID " + std::to_string(query.id));
    recordEventLocked(storedSnapshot,
                      "query.queued",
                      "Added to the shared query queue");
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

std::vector<Query> QueryManager::getPendingQueries() const {
    return queue_.orderedQueries();
}

std::vector<QueryEvent> QueryManager::getEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

std::vector<WorkerSnapshot> QueryManager::getWorkers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workers_;
}

std::size_t QueryManager::queueSize() const {
    return queue_.size();
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
    const Query& query,
    std::size_t workerIndex,
    std::chrono::steady_clock::time_point startedAt) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = queries_.find(query.id);
    if (found == queries_.end()) {
        return;
    }
    found->second.status = QueryStatus::Running;
    found->second.startTime = startedAt;
    found->second.startedAt = std::chrono::system_clock::now();
    found->second.workerId = "worker-" + std::to_string(workerIndex + 1);
    auto& worker = workers_.at(workerIndex);
    worker.busy = true;
    worker.queryId = query.id;
    worker.operation = found->second.operation;
    worker.startedAt = found->second.startedAt;
    const auto policyDescription =
        policy_ == SchedulingPolicy::PRIORITY
            ? "priority then FCFS arrival sequence"
            : "FCFS arrival sequence";
    recordEventLocked(found->second,
                      "query.scheduled",
                      "Selected by " + std::string(policyDescription),
                      worker.workerId);
    recordEventLocked(found->second,
                      "worker.started",
                      "Worker began query execution",
                      worker.workerId);
    recordEventLocked(found->second,
                      "database.started",
                      "Database execution started",
                      worker.workerId);
}

void QueryManager::markCompleted(
    const Query& query,
    std::size_t workerIndex,
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
        if (snapshot.startedAt.time_since_epoch().count() == 0) {
            snapshot.startedAt = std::chrono::system_clock::now();
        }
        snapshot.finishedAt = std::chrono::system_clock::now();
        snapshot.workerId = "worker-" + std::to_string(workerIndex + 1);
        snapshot.columns = result.columns;
        snapshot.rows = result.rows;
        if (result.affectedRows.has_value()) {
            snapshot.affectedRows = *result.affectedRows;
            snapshot.affectedRowsAvailable = true;
        }
        snapshot.result = result.message;
        snapshot.error = error;
        auto& worker = workers_.at(workerIndex);
        worker.busy = false;
        worker.queryId = 0;
        worker.operation.clear();
        worker.startedAt = {};
        if (error.empty()) {
            ++worker.completedQueries;
        }
        recordEventLocked(snapshot,
                          error.empty() ? "database.completed" : "database.failed",
                          error.empty() ? result.message : error,
                          worker.workerId);
        recordEventLocked(snapshot,
                          error.empty() ? "query.completed" : "query.failed",
                          error.empty() ? "Result delivered" : "Execution failed",
                          worker.workerId);
    }
    idleCondition_.notify_all();
}

void QueryManager::recordEventLocked(const QuerySnapshot& query,
                                     const std::string& type,
                                     const std::string& detail,
                                     const std::string& workerId) {
    QueryEvent event;
    event.eventId = nextEventId_++;
    event.timestamp = std::chrono::system_clock::now();
    event.type = type;
    event.queryId = query.id;
    event.userId = query.userId;
    event.workerId = workerId;
    event.detail = detail;
    events_.push_back(std::move(event));
    constexpr std::size_t maxEvents = 2000;
    if (events_.size() > maxEvents) {
        events_.erase(events_.begin(), events_.begin() +
                                         (events_.size() - maxEvents));
    }
}

}  // namespace queryflow
