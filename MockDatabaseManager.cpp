#include "MockDatabaseManager.h"

#include <thread>

MockDatabaseManager::MockDatabaseManager(std::chrono::milliseconds simulatedLatency)
    : simulatedLatency_(simulatedLatency) {}

void MockDatabaseManager::execute(const Query& query) {
    // Pretend this is a MySQL round-trip.
    std::this_thread::sleep_for(simulatedLatency_);

    std::lock_guard<std::mutex> lock(mutex_);
    executedIds_.push_back(query.id);
}

std::vector<int> MockDatabaseManager::executedIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return executedIds_;
}
