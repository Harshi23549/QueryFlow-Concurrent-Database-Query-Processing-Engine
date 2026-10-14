#include <charconv>
#include <cstddef>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#include "queryflow/database/MockDatabaseManager.h"
#include "queryflow/manager/QueryManager.h"
#include "queryflow/metrics/QueryMetrics.h"

namespace {

void printHelp() {
    std::cout
        << "Commands:\n"
        << "  Enter SQL text           Submit a query at priority 0\n"
        << "  /submit <priority> <SQL> Submit with an explicit priority\n"
        << "  /status                  List submitted query statuses\n"
        << "  /wait                    Wait for all work and print results\n"
        << "  /help                    Show this help\n"
        << "  /quit                    Drain queued work and exit\n"
        << "\n"
        << "This build uses a demo database backend. It simulates execution; it does\n"
        << "not send SQL to MySQL or any other database.\n";
}

bool parsePriority(const std::string& text, int& priority) {
    const char* first = text.data();
    const char* last = first + text.size();
    const auto parsed = std::from_chars(first, last, priority);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

void printStatuses(const queryflow::QueryManager& manager) {
    const auto queries = manager.getQueries();
    if (queries.empty()) {
        std::cout << "No queries submitted.\n";
        return;
    }

    for (const auto& query : queries) {
        std::cout << "Q" << query.id << " [" << queryflow::toString(query.status)
                  << "] priority=" << query.priority << " | " << query.sql << '\n';
    }
}

void printResults(const queryflow::QueryManager& manager,
                  std::set<int>& reportedQueryIds) {
    for (const auto& query : manager.getQueries()) {
        if (reportedQueryIds.count(query.id) != 0) {
            continue;
        }
        if (query.status == queryflow::QueryStatus::Failed) {
            std::cout << "Q" << query.id << " failed: " << query.error << '\n';
            reportedQueryIds.insert(query.id);
            continue;
        }
        if (query.status != queryflow::QueryStatus::Completed) {
            continue;
        }

        const auto metrics = queryflow::QueryMetrics::fromSnapshot(query);
        std::cout << "Q" << query.id << " completed: " << query.result << '\n'
                  << "  waiting=" << metrics.waitingTimeMs() << " ms"
                  << ", execution=" << metrics.executionTimeMs() << " ms"
                  << ", response=" << metrics.responseTimeMs() << " ms\n";
        reportedQueryIds.insert(query.id);
    }
}

void submitSql(queryflow::QueryManager& manager,
               const std::string& sql,
               int priority) {
    try {
        const auto query = manager.submit(sql, priority);
        std::cout << "Submitted Q" << query.id << " (priority " << priority << ").\n";
    } catch (const std::exception& exception) {
        std::cerr << "Could not submit query: " << exception.what() << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::size_t workerCount = 4;
    queryflow::SchedulingPolicy policy = queryflow::SchedulingPolicy::FCFS;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            std::cout << "Usage: QueryFlow [--workers N] [--policy fcfs|priority]\n";
            printHelp();
            return 0;
        }
        if (argument == "--workers") {
            if (i + 1 >= argc) {
                std::cerr << "--workers requires a positive integer\n";
                return 2;
            }
            const std::string workerText = argv[++i];
            std::size_t parsed = 0;
            const auto parsedCount = std::from_chars(
                workerText.data(), workerText.data() + workerText.size(), parsed);
            if (parsedCount.ec != std::errc{} ||
                parsedCount.ptr != workerText.data() + workerText.size() ||
                parsed == 0) {
                std::cerr << "--workers requires a positive integer\n";
                return 2;
            }
            workerCount = parsed;
            continue;
        }
        if (argument == "--policy") {
            if (i + 1 >= argc) {
                std::cerr << "--policy requires fcfs or priority\n";
                return 2;
            }
            const std::string policyName = argv[++i];
            if (policyName == "fcfs") {
                policy = queryflow::SchedulingPolicy::FCFS;
            } else if (policyName == "priority") {
                policy = queryflow::SchedulingPolicy::PRIORITY;
            } else {
                std::cerr << "Unknown policy '" << policyName
                          << "'; choose fcfs or priority\n";
                return 2;
            }
            continue;
        }
        std::cerr << "Unknown option: " << argument << '\n';
        return 2;
    }

    queryflow::MockDatabaseManager database;
    queryflow::QueryManager manager(workerCount, policy, database);
    std::set<int> reportedQueryIds;

    std::cout << "QueryFlow CLI (workers=" << workerCount << ", policy="
              << (policy == queryflow::SchedulingPolicy::FCFS ? "FCFS" : "priority")
              << ")\n"
              << "Backend: demo only; queries are simulated, not executed by MySQL.\n"
              << "Enter /help for commands or /quit to exit.\n";

    std::string line;
    while (std::cout << "queryflow> " && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        if (line == "/quit" || line == "/exit") {
            break;
        }
        if (line == "/help") {
            printHelp();
            continue;
        }
        if (line == "/status") {
            printStatuses(manager);
            continue;
        }
        if (line == "/wait") {
            manager.waitUntilIdle();
            printResults(manager, reportedQueryIds);
            continue;
        }
        if (line.rfind("/submit", 0) == 0) {
            std::istringstream command(line);
            std::string name;
            std::string priorityText;
            command >> name >> priorityText;
            std::string sql;
            std::getline(command >> std::ws, sql);
            int priorityValue = 0;
            if (name != "/submit" || priorityText.empty() || sql.empty() ||
                !parsePriority(priorityText, priorityValue)) {
                std::cerr << "Usage: /submit <integer-priority> <SQL>\n";
                continue;
            }
            submitSql(manager, sql, priorityValue);
            continue;
        }
        if (line.front() == '/') {
            std::cerr << "Unknown command. Enter /help for available commands.\n";
            continue;
        }
        submitSql(manager, line, 0);
    }

    manager.shutdown();
    printResults(manager, reportedQueryIds);
    return 0;
}
