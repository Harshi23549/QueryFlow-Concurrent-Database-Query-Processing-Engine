#include <cassert>
#include <iostream>
#include <vector>

#include "queryflow/queue/SharedQueryQueue.h"
#include "queryflow/scheduler/Scheduler.h"

int main() {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::time_point{};
    std::vector<queryflow::Query> queries = {
        queryflow::Query(1, "SELECT 1", 1),
        queryflow::Query(2, "SELECT 2", 3),
        queryflow::Query(3, "SELECT 3", 3)
    };
    queries[0].submitTime = start;
    queries[1].submitTime = start + std::chrono::milliseconds(2);
    queries[2].submitTime = start + std::chrono::milliseconds(1);

    queryflow::Scheduler fcfs(queryflow::SchedulingPolicy::FCFS);
    assert(fcfs.selectNext(queries) == 0);

    queryflow::Scheduler priority(queryflow::SchedulingPolicy::PRIORITY);
    assert(priority.selectNext(queries) == 2);

    queries[1].priority = 3;
    queries[2].priority = 3;
    assert(priority.selectNext(queries) == 2);

    std::vector<queryflow::Query> empty;
    assert(fcfs.selectNext(empty) == -1);

    queryflow::SharedQueryQueue priorityQueue(queryflow::SchedulingPolicy::PRIORITY);
    assert(priorityQueue.push(queryflow::Query(10, "low", 1)));
    assert(priorityQueue.push(queryflow::Query(11, "high", 9)));
    queryflow::Query selected;
    assert(priorityQueue.waitAndPop(selected));
    assert(selected.id == 11);
    assert(priorityQueue.waitAndPop(selected));
    assert(selected.id == 10);
    priorityQueue.requestStop();
    assert(!priorityQueue.push(queryflow::Query(12, "rejected")));
    assert(!priorityQueue.waitAndPop(selected));

    std::cout << "All scheduler and queue selection tests passed!\n";
    return 0;
}
