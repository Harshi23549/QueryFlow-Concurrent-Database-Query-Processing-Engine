#pragma once

#include <chrono>
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
    std::string sql;
    int priority = 0;
    std::chrono::steady_clock::time_point submitTime;

    Query() : submitTime(std::chrono::steady_clock::now()) {}

    Query(int id_, std::string sql_, int priority_ = 0)
        : id(id_),
          sql(std::move(sql_)),
          priority(priority_),
          submitTime(std::chrono::steady_clock::now()) {}
};

struct QuerySnapshot {
    int id = 0;
    std::string sql;
    int priority = 0;
    QueryStatus status = QueryStatus::Queued;
    std::chrono::steady_clock::time_point submitTime;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point finishTime;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::string result;
    std::string error;
};

}  // namespace queryflow
