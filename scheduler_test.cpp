#include "scheduler.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using Clock = std::chrono::steady_clock;

QueryTask makeQuery(
    const std::string& id,
    int priority,
    int submittedMs) {

    QueryTask q;
    q.queryId = id;
    q.sql = "SELECT 1";
    q.priority = priority;
    q.submittedAt = Clock::time_point{} +
                    std::chrono::milliseconds(submittedMs);
    return q;
}

int main() {
    std::vector<QueryTask> queries = {
        makeQuery("Q1", 1, 0),
        makeQuery("Q2", 3, 2),
        makeQuery("Q3", 3, 1)
    };

    // Test FCFS: earliest submission wins.
    Scheduler fcfs(SchedulingPolicy::FCFS);
    assert(fcfs.selectNext(queries) == 0);

    // Test priority: highest priority wins.
    Scheduler priority(SchedulingPolicy::PRIORITY);
    assert(priority.selectNext(queries) == 2);

    // Test tie-breaking: Q3 was submitted before Q2.
    queries[1].priority = 3;
    queries[2].priority = 3;
    assert(priority.selectNext(queries) == 2);

    // Test empty queue.
    std::vector<QueryTask> empty;
    assert(fcfs.selectNext(empty) == -1);

    // Test time calculations.
    QueryTask q = makeQuery("Q4", 1, 0);
    q.startedAt = Clock::time_point{} +
                  std::chrono::milliseconds(5);
    q.finishedAt = Clock::time_point{} +
                   std::chrono::milliseconds(12);

    assert(std::abs(Scheduler::waitingTimeMs(q) - 5.0) < 0.001);
    assert(std::abs(Scheduler::executionTimeMs(q) - 7.0) < 0.001);
    assert(std::abs(Scheduler::responseTimeMs(q) - 12.0) < 0.001);

    std::cout << "All scheduler tests passed!\n";
    return 0;
}