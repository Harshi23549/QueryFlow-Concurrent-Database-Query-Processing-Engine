#pragma once

#include <chrono>
#include <mutex>
#include <vector>

#include "queryflow/database/IDatabaseManager.h"

namespace queryflow {

class MockDatabaseManager : public IDatabaseManager {
public:
    explicit MockDatabaseManager(std::chrono::milliseconds simulatedLatency = std::chrono::milliseconds(2));

    void execute(const Query& query) override;
    std::vector<int> executedIds() const;

private:
    std::chrono::milliseconds simulatedLatency_;
    mutable std::mutex mutex_;
    std::vector<int> executedIds_;
};

}  // namespace queryflow
