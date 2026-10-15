#include "Database.hpp"

#include <exception>
#include <iostream>
#include <optional>
#include <string>

int main() {
    try {
        queryflow::db::Database database(
            queryflow::db::ConnectionSettings::fromEnvironment());
        if (!database.ping()) {
            std::cerr << "MySQL ping returned an unexpected result\n";
            return 1;
        }

        // Exercise bound values and transaction methods without keeping demo rows.
        auto transaction = database.beginTransaction();
        database.executePrepared(
            "INSERT INTO query_execution_log "
            "(scheduling_method, priority, query_label) VALUES (?, ?, ?)",
            {std::string("FCFS"), std::string("0"), std::string("cpp_adapter_demo")});

        const auto idResult = database.execute("SELECT LAST_INSERT_ID() AS query_id");
        if (idResult.rows.empty() || !idResult.rows.front().front().has_value()) {
            throw std::runtime_error("Could not read the inserted query ID");
        }
        const auto queryId = *idResult.rows.front().front();

        database.executePrepared(
            "UPDATE query_execution_log SET query_status='running', worker_id=?, "
            "started_at=CURRENT_TIMESTAMP(6) WHERE query_id=?",
            {std::string("worker-demo"), queryId});
        database.executePrepared(
            "UPDATE query_execution_log SET query_status='succeeded', "
            "finished_at=CURRENT_TIMESTAMP(6), affected_rows=1 WHERE query_id=?",
            {queryId});

        const auto metrics = database.executePrepared(
            "SELECT query_id, query_status, wait_time_us, execution_time_us, "
            "response_time_us FROM query_execution_metrics WHERE query_id=?",
            {queryId});
        for (std::size_t i = 0; i < metrics.columns.size(); ++i) {
            std::cout << (i == 0 ? "" : " | ") << metrics.columns[i] << "=";
            const auto& value = metrics.rows.front()[i];
            std::cout << (value.has_value() ? *value : "NULL");
        }
        std::cout << "\nRolling back demo row.\n";
        transaction.rollback();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Database smoke test failed: " << error.what() << '\n';
        return 1;
    }
}
