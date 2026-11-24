#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace queryflow {

enum class QueryStatus {
    Queued,
    Running,
    Completed,
    Failed
};

inline const char* toString(QueryStatus status) {
    switch (status) {
    case QueryStatus::Queued:
        return "queued";
    case QueryStatus::Running:
        return "running";
    case QueryStatus::Completed:
        return "completed";
    case QueryStatus::Failed:
        return "failed";
    }
    return "unknown";
}

struct Query {
    int id = 0;
    std::uint64_t arrivalSequence = 0;
    std::string sql;
    std::vector<std::optional<std::string>> parameters;
    std::string userId;
    std::string sessionLabel;
    std::string operation = "custom";
    int priority = 0;
    std::chrono::steady_clock::time_point submitTime;
    std::chrono::system_clock::time_point submittedAt;

    Query()
        : submitTime(std::chrono::steady_clock::now()),
          submittedAt(std::chrono::system_clock::now()) {}

    Query(int id_, std::string sql_, int priority_ = 0)
        : id(id_),
          sql(std::move(sql_)),
          priority(priority_),
          submitTime(std::chrono::steady_clock::now()),
          submittedAt(std::chrono::system_clock::now()) {}
};

struct QuerySnapshot {
    int id = 0;
    std::uint64_t arrivalSequence = 0;
    std::string sql;
    std::string userId;
    std::string sessionLabel;
    std::string operation = "custom";
    int priority = 0;
    QueryStatus status = QueryStatus::Queued;
    std::chrono::steady_clock::time_point submitTime;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point finishTime;
    std::chrono::system_clock::time_point submittedAt;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point finishedAt;
    std::string workerId;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::size_t affectedRows = 0;
    bool affectedRowsAvailable = false;
    std::string result;
    std::string error;
};

struct QueryEvent {
    std::uint64_t eventId = 0;
    std::chrono::system_clock::time_point timestamp;
    std::string type;
    int queryId = 0;
    std::string userId;
    std::string workerId;
    std::string detail;
};

struct WorkerSnapshot {
    std::string workerId;
    bool busy = false;
    int queryId = 0;
    std::string operation;
    std::chrono::system_clock::time_point startedAt;
    std::size_t completedQueries = 0;
};

}  // namespace queryflow
