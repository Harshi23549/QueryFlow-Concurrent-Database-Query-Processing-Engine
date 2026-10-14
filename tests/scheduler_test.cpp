#include "queryflow/scheduler/Scheduler.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

queryflow::QueryTask makeQuery(const std::string& id, int priority, int submittedMs) {
    queryflow::QueryTask q;
    q.queryId = id;
    q.sql = "SELECT 1";
    q.priority = priority;
    q.submittedAt = Clock::time_point{} + std::chrono::milliseconds(submittedMs);
    return q;
}

}  // namespace

int main() {
    std::vector<queryflow::QueryTask> queries = {
        makeQuery("Q1", 1, 0),
        makeQuery("Q2", 3, 2),
        makeQuery("Q3", 3, 1)
    };

    queryflow::Scheduler fcfs(queryflow::SchedulingPolicy::FCFS);
    assert(fcfs.selectNext(queries) == 0);

    queryflow::Scheduler priority(queryflow::SchedulingPolicy::PRIORITY);
    assert(priority.selectNext(queries) == 2);

    queries[1].priority = 3;
    queries[2].priority = 3;
    assert(priority.selectNext(queries) == 2);

    std::vector<queryflow::QueryTask> empty;
    assert(fcfs.selectNext(empty) == -1);

    queryflow::QueryTask q = makeQuery("Q4", 1, 0);
    q.startedAt = Clock::time_point{} + std::chrono::milliseconds(5);
    q.finishedAt = Clock::time_point{} + std::chrono::milliseconds(12);

    assert(std::abs(queryflow::Scheduler::waitingTimeMs(q) - 5.0) < 0.001);
    assert(std::abs(queryflow::Scheduler::executionTimeMs(q) - 7.0) < 0.001);
    assert(std::abs(queryflow::Scheduler::responseTimeMs(q) - 12.0) < 0.001);

    std::cout << "All scheduler tests passed!\n";
    return 0;
}
