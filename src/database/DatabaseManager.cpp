#include "queryflow/database/DatabaseManager.h"

#include <stdexcept>

#ifdef QUERYFLOW_WITH_MYSQL
#include "Database.hpp"

#include <map>
#include <mutex>
#include <thread>
#include <utility>
#endif

namespace queryflow {

struct DatabaseManager::State {
#ifdef QUERYFLOW_WITH_MYSQL
    db::ConnectionSettings settings = db::ConnectionSettings::fromEnvironment();
    std::mutex mutex;
    std::map<std::thread::id, std::unique_ptr<db::Database>> connections;

    db::Database& connectionForCurrentThread() {
        std::lock_guard<std::mutex> lock(mutex);
        const auto threadId = std::this_thread::get_id();
        auto& connection = connections[threadId];
        if (!connection) {
            connection = std::make_unique<db::Database>(settings);
        }
        return *connection;
    }
#endif
};

DatabaseManager::DatabaseManager()
    : state_(std::make_unique<State>()) {}

DatabaseManager::~DatabaseManager() = default;

DatabaseResult DatabaseManager::execute(const Query& query) {
#ifdef QUERYFLOW_WITH_MYSQL
    const auto databaseResult = state_->connectionForCurrentThread().execute(query.sql);
    DatabaseResult result;
    result.columns = databaseResult.columns;
    if (databaseResult.columns.empty()) {
        result.affectedRows = static_cast<std::size_t>(databaseResult.affectedRows);
    }
    result.rows.reserve(databaseResult.rows.size());
    for (const auto& databaseRow : databaseResult.rows) {
        auto& row = result.rows.emplace_back();
        row.reserve(databaseRow.size());
        for (const auto& value : databaseRow) {
            row.push_back(value.value_or("NULL"));
        }
    }
     result.message = databaseResult.columns.empty()
                         ? "Statement completed; affected rows: " +
                               (result.affectedRows.has_value()
                                    ? std::to_string(result.affectedRows.value())
                                    : "N/A")
                         : "Query returned " + std::to_string(result.rows.size()) +
                               " row(s)";
    return result;
#else
    (void)query;
    throw std::runtime_error(
        "MySQL support is not enabled; rebuild with -DQUERYFLOW_ENABLE_MYSQL=ON");
#endif
}

bool DatabaseManager::ping() {
#ifdef QUERYFLOW_WITH_MYSQL
    return state_->connectionForCurrentThread().ping();
#else
    throw std::runtime_error(
        "MySQL support is not enabled; rebuild with -DQUERYFLOW_ENABLE_MYSQL=ON");
#endif
}

}  // namespace queryflow
