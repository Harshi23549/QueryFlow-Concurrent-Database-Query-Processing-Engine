#include "queryflow/manager/QueryManager.h"

namespace queryflow {

QueryManager::QueryManager()
    : scheduler_(SchedulingPolicy::FCFS) {}

QueryManager::~QueryManager() = default;

Query QueryManager::submit(const std::string& sql) {
    static int nextId = 1;
    Query query(nextId++, sql);
    queue_.push(query);
    return query;
}

std::vector<Query> QueryManager::getCompletedQueries() const {
    return {};
}

}  // namespace queryflow
