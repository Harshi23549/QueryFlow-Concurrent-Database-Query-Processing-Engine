#include <iostream>

#include "queryflow/scheduler/Scheduler.h"

int main() {
    queryflow::Scheduler scheduler(queryflow::SchedulingPolicy::FCFS);
    std::cout << "QueryFlow engine initialized.\n";
    std::cout << "Scheduling policy: FCFS\n";
    (void)scheduler;
    return 0;
}
