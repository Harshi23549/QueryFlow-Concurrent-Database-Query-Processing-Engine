#pragma once

#include <string>

namespace queryflow {

struct QueryFlowConfig {
    int maxWorkers = 4;
    bool enablePriorityScheduling = true;
    std::string backend = "mock";
    std::string databaseName = "queryflow";
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "queryflow_app";
    std::string password;

    static QueryFlowConfig fromEnvironment();
};


}  // namespace queryflow
