#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "queryflow/core/Query.h"

namespace queryflow {

struct DatabaseResult {
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::optional<std::size_t> affectedRows;
    std::string message;
};

class IDatabaseManager {
public:
    virtual ~IDatabaseManager() = default;
    virtual DatabaseResult execute(const Query& query) = 0;
};

}  // namespace queryflow
