#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <chrono>
#include <string>
#include <vector>

enum class SchedulingPolicy {
    FCFS,
    PRIORITY
};

struct QueryTask {
    std::string queryId;
    std::string sql;
    int priority = 0;

    std::chrono::steady_clock::time_point submittedAt;
    std::chrono::steady_clock::time_point startedAt;
    std::chrono::steady_clock::time_point finishedAt;
};

class Scheduler {
public:
    explicit Scheduler(SchedulingPolicy policy);

    void setPolicy(SchedulingPolicy policy);

    // Returns the index of the next query, or -1 if empty.
    int selectNext(const std::vector<QueryTask>& queries) const;

    static double waitingTimeMs(const QueryTask& query);
    static double executionTimeMs(const QueryTask& query);
    static double responseTimeMs(const QueryTask& query);

private:
    SchedulingPolicy policy_;
};

#endif