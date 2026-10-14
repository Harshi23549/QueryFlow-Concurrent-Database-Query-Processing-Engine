#pragma once

#include <string>
#include <vector>

#include "queryflow/core/Query.h"
#include "queryflow/queue/SharedQueryQueue.h"
#include "queryflow/scheduler/Scheduler.h"

namespace queryflow {

class QueryManager {
public:
    QueryManager();
    ~QueryManager();

    Query submit(const std::string& sql);
    std::vector<Query> getCompletedQueries() const;

private:
    SharedQueryQueue queue_;
    Scheduler scheduler_;
};

}  // namespace queryflow
