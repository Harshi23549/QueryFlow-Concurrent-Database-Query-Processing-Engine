#pragma once

#include "queryflow/core/Query.h"
#include "queryflow/database/IDatabaseManager.h"

namespace queryflow {

class DatabaseManager : public IDatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager() override = default;

    DatabaseResult execute(const Query& query) override;
};

}  // namespace queryflow
