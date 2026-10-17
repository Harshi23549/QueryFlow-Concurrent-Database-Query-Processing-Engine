#include "queryflow/database/MockDatabaseManager.h"

#include <thread>

namespace queryflow {

MockDatabaseManager::MockDatabaseManager(std::chrono::milliseconds simulatedLatency)
    : simulatedLatency_(simulatedLatency) {}

DatabaseResult MockDatabaseManager::execute(const Query& query) {
    std::this_thread::sleep_for(simulatedLatency_);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        executedIds_.push_back(query.id);
    }

    DatabaseResult result;
    result.message = "Demo backend simulated execution; SQL was not sent to a database.";
    return result;
}

std::vector<int> MockDatabaseManager::executedIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executedIds_;
}

}  // namespace queryflow
