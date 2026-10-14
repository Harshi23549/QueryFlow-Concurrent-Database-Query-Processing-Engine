#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>

#include "queryflow/database/MockDatabaseManager.h"
#include "queryflow/manager/QueryManager.h"
#include "queryflow/metrics/QueryMetrics.h"

namespace {

class FailingDatabaseManager : public queryflow::IDatabaseManager {
public:
    queryflow::DatabaseResult execute(const queryflow::Query&) override {
        throw std::runtime_error("synthetic database failure");
    }
};

}  // namespace

int main() {
    queryflow::MockDatabaseManager database(std::chrono::milliseconds(1));
    queryflow::QueryManager manager(
        2, queryflow::SchedulingPolicy::PRIORITY, database);

    const auto first = manager.submit("SELECT 1", 1);
    const auto second = manager.submit("SELECT 2", 5);
    assert(first.id == 1);
    assert(second.id == 2);

    bool rejectedBlankQuery = false;
    try {
        manager.submit(" \t\n");
    } catch (const std::invalid_argument&) {
        rejectedBlankQuery = true;
    }
    assert(rejectedBlankQuery);

    manager.waitUntilIdle();
    const auto completed = manager.getQueries();
    assert(completed.size() == 2);
    for (const auto& query : completed) {
        assert(query.status == queryflow::QueryStatus::Completed);
        assert(query.error.empty());
        assert(query.result.find("simulated") != std::string::npos);
        const auto metrics = queryflow::QueryMetrics::fromSnapshot(query);
        assert(metrics.waitingTimeMs() >= 0.0);
        assert(metrics.executionTimeMs() >= 0.0);
        assert(metrics.responseTimeMs() >= metrics.executionTimeMs());
    }

    assert(database.executedIds().size() == 2);
    assert(!manager.getQuery(999).has_value());
    manager.shutdown();

    bool rejectedAfterShutdown = false;
    try {
        manager.submit("SELECT 3");
    } catch (const std::logic_error&) {
        rejectedAfterShutdown = true;
    }
    assert(rejectedAfterShutdown);

    FailingDatabaseManager failingDatabase;
    queryflow::QueryManager failingManager(
        1, queryflow::SchedulingPolicy::FCFS, failingDatabase);
    const auto failedQuery = failingManager.submit("SELECT fails");
    failingManager.waitUntilIdle();
    const auto failedSnapshot = failingManager.getQuery(failedQuery.id);
    assert(failedSnapshot.has_value());
    assert(failedSnapshot->status == queryflow::QueryStatus::Failed);
    assert(failedSnapshot->error == "synthetic database failure");

    std::cout << "Engine lifecycle tests passed!\n";
    return 0;
}
