#pragma once

#include <memory>

#include "queryflow/core/Query.h"
#include "queryflow/database/IDatabaseManager.h"

namespace queryflow {

class DatabaseManager : public IDatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager() override;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    DatabaseResult execute(const Query& query) override;
    bool ping();

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace queryflow
