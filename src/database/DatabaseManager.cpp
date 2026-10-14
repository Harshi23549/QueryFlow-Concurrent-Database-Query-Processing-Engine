#include "queryflow/database/DatabaseManager.h"

#include <stdexcept>

namespace queryflow {

DatabaseManager::DatabaseManager() = default;

DatabaseResult DatabaseManager::execute(const Query& query) {
    (void)query;
    throw std::runtime_error(
        "MySQL execution is not configured; use the demo backend or implement the MySQL connector");
}

}  // namespace queryflow
