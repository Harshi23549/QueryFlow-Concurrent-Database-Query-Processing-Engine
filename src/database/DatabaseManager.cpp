#include "queryflow/database/DatabaseManager.h"

#include <algorithm>
#include <stdexcept>

#ifdef QUERYFLOW_WITH_MYSQL
#include "Database.hpp"
#include "queryflow/security/PasswordHash.h"

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
    auto& database = state_->connectionForCurrentThread();
    db::QueryResult databaseResult;
    if (query.operation == "checkout") {
        if (query.parameters.size() != 7 ||
            std::any_of(query.parameters.begin(), query.parameters.end(),
                        [](const auto& value) { return !value.has_value(); })) {
            throw std::invalid_argument("Checkout is missing required order details");
        }
        std::size_t parsedLength = 0;
        const auto customerId = std::stoull(*query.parameters.front(), &parsedLength);
        if (parsedLength != query.parameters.front()->size()) {
            throw std::invalid_argument("Checkout customer ID is invalid");
        }
        databaseResult = database.checkout(customerId,
                                           *query.parameters[1],
                                           *query.parameters[2],
                                           *query.parameters[3],
                                           *query.parameters[4],
                                           *query.parameters[5],
                                           *query.parameters[6]);
    } else if (query.operation == "account-register") {
        if (query.parameters.size() != 4 ||
            std::any_of(query.parameters.begin(), query.parameters.end(),
                        [](const auto& value) { return !value.has_value(); })) {
            throw std::invalid_argument("Account creation request is incomplete");
        }
        auto transaction = database.beginTransaction();
        const auto inserted = database.executePrepared(
            "INSERT INTO customers (full_name, email, password_hash, password_salt) "
            "VALUES (?, ?, ?, ?)",
            query.parameters);
        if (!inserted.affectedRows.has_value() || *inserted.affectedRows != 1) {
            throw std::runtime_error("Could not create the customer account");
        }
        const auto generated = database.execute("SELECT LAST_INSERT_ID()");
        if (generated.rows.empty() || generated.rows.front().empty() ||
            !generated.rows.front().front().has_value()) {
            throw std::runtime_error("Could not obtain the customer ID");
        }
        const auto customerId = *generated.rows.front().front();
        const auto cart = database.executePrepared(
            "INSERT INTO shopping_carts (customer_id) VALUES (?)",
            {customerId});
        if (!cart.affectedRows.has_value() || *cart.affectedRows != 1) {
            throw std::runtime_error("Could not create the customer cart");
        }
        transaction.commit();
        DatabaseResult result;
        result.columns = {"customer_id"};
        result.rows = {{customerId}};
        result.message = "Customer account created";
        return result;
    } else if (query.operation == "account-login") {
        DatabaseResult result;
        result.columns = {"customer_id", "full_name", "email"};
        if (query.parameters.size() != 2 || !query.parameters[0].has_value() ||
            !query.parameters[1].has_value()) {
            throw std::invalid_argument("Sign-in request is incomplete");
        }
        const auto stored = database.executePrepared(
            "SELECT customer_id, full_name, email, password_hash, password_salt "
            "FROM customers WHERE email = ?",
            {query.parameters[0]});
        if (stored.rows.size() == 1 && stored.rows.front().size() == 5 &&
            stored.rows.front()[3].has_value() &&
            stored.rows.front()[4].has_value() &&
            verifyPassword(*query.parameters[1],
                           *stored.rows.front()[4],
                           *stored.rows.front()[3])) {
            result.rows.emplace_back();
            for (std::size_t column = 0; column < 3; ++column) {
                result.rows.back().push_back(
                    stored.rows.front()[column].value_or("NULL"));
            }
        }
        result.message = result.rows.empty() ? "Credentials rejected"
                                             : "Customer signed in";
        return result;
    } else if (query.parameters.empty()) {
        databaseResult = database.execute(query.sql);
    } else {
        databaseResult = database.executePrepared(query.sql, query.parameters);
    }
    DatabaseResult result;
    result.columns = databaseResult.columns;
    if (databaseResult.affectedRows.has_value()) {
        result.affectedRows = static_cast<std::size_t>(*databaseResult.affectedRows);
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
                             std::to_string(result.affectedRows.value_or(0))
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
