#pragma once

#include <string>

namespace queryflow {

struct QueryFlowConfig {
    int maxWorkers = 4;
    bool enablePriorityScheduling = false;
    std::string databaseName = "queryflow";
    std::string host = "127.0.0.1";
    int port = 3306;
};

}  // namespace queryflow
