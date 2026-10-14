#pragma once

#include "queryflow/core/Query.h"

namespace queryflow {

class IDatabaseManager {
public:
    virtual ~IDatabaseManager() = default;
    virtual void execute(const Query& query) = 0;
};

}  // namespace queryflow
