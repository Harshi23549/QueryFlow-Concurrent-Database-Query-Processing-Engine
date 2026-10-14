#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "queryflow/core/Query.h"

namespace queryflow {

struct DatabaseResult {
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::size_t affectedRows = 0;
    std::string message;
};

class IDatabaseManager {
public:
    virtual ~IDatabaseManager() = default;
    virtual DatabaseResult execute(const Query& query) = 0;
};

}  // namespace queryflow
