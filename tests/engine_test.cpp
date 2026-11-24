#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "queryflow/database/MockDatabaseManager.h"
#include "queryflow/manager/QueryManager.h"
#include "queryflow/metrics/QueryMetrics.h"
#include "queryflow/security/PasswordHash.h"

namespace {

class FailingDatabaseManager : public queryflow::IDatabaseManager {
public:
    queryflow::DatabaseResult execute(const queryflow::Query&) override {
        throw std::runtime_error("synthetic database failure");
    }
};

class ResultDatabaseManager : public queryflow::IDatabaseManager {
public:
    queryflow::DatabaseResult execute(const queryflow::Query&) override {
        queryflow::DatabaseResult result;
        result.columns = {"answer"};
        result.rows = {{"42"}};
        return result;
    }
};

class FirstQueryGateDatabase : public queryflow::IDatabaseManager {
public:
    queryflow::DatabaseResult execute(const queryflow::Query& query) override {
        std::unique_lock<std::mutex> lock(mutex_);
        executionOrder_.push_back(query.id);
        if (query.id == 1) {
            firstQueryStarted_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this] { return releaseFirstQuery_; });
        }
        return {};
    }

    void waitForFirstQuery() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return firstQueryStarted_; });
    }

    void releaseFirstQuery() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            releaseFirstQuery_ = true;
        }
        condition_.notify_all();
    }

    std::vector<int> executionOrder() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return executionOrder_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<int> executionOrder_;
    bool firstQueryStarted_ = false;
    bool releaseFirstQuery_ = false;
};

}  // namespace

int main() {
    const auto [passwordHash, passwordSalt] =
        queryflow::hashPassword("classroom-password");
    assert(passwordHash.size() == 128);
    assert(passwordSalt.size() == 32);
    assert(queryflow::verifyPassword(
        "classroom-password", passwordSalt, passwordHash));
    assert(!queryflow::verifyPassword(
        "incorrect-password", passwordSalt, passwordHash));

    queryflow::MockDatabaseManager database(std::chrono::milliseconds(1));
    queryflow::QueryManager manager(
        2, queryflow::SchedulingPolicy::PRIORITY, database);

    const auto first = manager.submit("SELECT 1", 1, "test-user", "tab A", "records");
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
    const auto workers = manager.getWorkers();
    assert(workers.size() == 2);
    assert(std::all_of(workers.begin(), workers.end(), [](const auto& worker) {
        return !worker.busy;
    }));
    const auto events = manager.getEvents();
    assert(events.size() >= 10);
    assert(events.front().type == "query.received");
    assert(events.front().userId == "test-user");
    const auto firstSnapshot = manager.getQuery(first.id);
    assert(firstSnapshot->arrivalSequence == 1);
    assert(firstSnapshot->sessionLabel == "tab A");
    assert(firstSnapshot->operation == "records");
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

    ResultDatabaseManager resultDatabase;
    queryflow::QueryManager resultManager(
        1, queryflow::SchedulingPolicy::FCFS, resultDatabase);
    const auto resultQuery = resultManager.submit("SELECT 42");
    resultManager.waitUntilIdle();
    const auto resultSnapshot = resultManager.getQuery(resultQuery.id);
    assert(resultSnapshot.has_value());
    assert(resultSnapshot->columns == std::vector<std::string>{"answer"});
    assert(resultSnapshot->rows.size() == 1);
    assert(resultSnapshot->rows.front().front() == "42");

    FirstQueryGateDatabase gateDatabase;
    queryflow::QueryManager nonPreemptiveManager(
        1, queryflow::SchedulingPolicy::PRIORITY, gateDatabase);
    const auto runningQuery = nonPreemptiveManager.submit("RUNNING", 0);
    gateDatabase.waitForFirstQuery();
    const auto queuedLow = nonPreemptiveManager.submit("LOW", 1);
    const auto queuedHigh = nonPreemptiveManager.submit("HIGH", 9);
    gateDatabase.releaseFirstQuery();
    nonPreemptiveManager.waitUntilIdle();
    assert(gateDatabase.executionOrder() ==
           std::vector<int>({runningQuery.id, queuedHigh.id, queuedLow.id}));
    const auto dispatchEvents = nonPreemptiveManager.getEvents();
    std::vector<int> dispatchedIds;
    for (const auto& event : dispatchEvents) {
        if (event.type == "query.scheduled") {
            dispatchedIds.push_back(event.queryId);
        }
    }
    assert(dispatchedIds ==
           std::vector<int>({runningQuery.id, queuedHigh.id, queuedLow.id}));
    const auto highPriorityDecision = std::find_if(
        dispatchEvents.begin(), dispatchEvents.end(), [&queuedHigh](const auto& event) {
            return event.type == "query.scheduled" && event.queryId == queuedHigh.id;
        });
    assert(highPriorityDecision != dispatchEvents.end());
    assert(highPriorityDecision->detail ==
           "Selected by priority then FCFS arrival sequence");

    queryflow::MockDatabaseManager concurrentDatabase(std::chrono::milliseconds(0));
    queryflow::QueryManager concurrentManager(
        4, queryflow::SchedulingPolicy::PRIORITY, concurrentDatabase);
    std::mutex submittedIdsMutex;
    std::vector<int> submittedIds;
    std::vector<std::thread> submitters;
    constexpr int producerCount = 8;
    constexpr int queriesPerProducer = 25;
    for (int producer = 0; producer < producerCount; ++producer) {
        submitters.emplace_back([&] {
            std::vector<int> localIds;
            for (int index = 0; index < queriesPerProducer; ++index) {
                localIds.push_back(concurrentManager.submit("SELECT 1").id);
            }
            std::lock_guard<std::mutex> lock(submittedIdsMutex);
            submittedIds.insert(submittedIds.end(), localIds.begin(), localIds.end());
        });
    }
    for (auto& submitter : submitters) {
        submitter.join();
    }
    concurrentManager.waitUntilIdle();
    std::sort(submittedIds.begin(), submittedIds.end());
    assert(submittedIds.size() == producerCount * queriesPerProducer);
    for (std::size_t index = 0; index < submittedIds.size(); ++index) {
        assert(submittedIds[index] == static_cast<int>(index + 1));
    }
    const auto concurrentSnapshots = concurrentManager.getQueries();
    assert(concurrentSnapshots.size() == submittedIds.size());
    std::vector<std::uint64_t> arrivalSequences;
    arrivalSequences.reserve(concurrentSnapshots.size());
    for (const auto& snapshot : concurrentSnapshots) {
        arrivalSequences.push_back(snapshot.arrivalSequence);
    }
    std::sort(arrivalSequences.begin(), arrivalSequences.end());
    for (std::size_t index = 0; index < arrivalSequences.size(); ++index) {
        assert(arrivalSequences[index] == index + 1);
    }

    std::cout << "Engine lifecycle tests passed!\n";
    return 0;
}
