#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#include "queryflow/batch/BatchRunner.h"
#include "queryflow/batch/SqlFileReader.h"
#include "queryflow/database/DatabaseManager.h"
#include "queryflow/database/IDatabaseManager.h"
#include "queryflow/database/MockDatabaseManager.h"
#include "queryflow/manager/QueryManager.h"
#include "queryflow/metrics/QueryMetrics.h"

namespace {

void printHelp() {
    std::cout
        << "Usage: QueryFlow [--workers N] [--policy fcfs|priority] [--backend demo|mysql]\n"
        << "                 [--db-host H] [--db-port P] [--db-name N] [--db-user U] [--db-password P]\n"
        << "                 [--file workload.sql] [--priority N] [--log-dir DIR]\n"
        << "\n"
        << "Batch mode executes every SQL statement from the explicitly supplied file,\n"
        << "then writes a human-readable log and CSV report.\n"
        << "\n"
        << "Commands:\n"
        << "  Enter SQL text           Submit a query at priority 0\n"
        << "  /submit <priority> <SQL> Submit with an explicit priority\n"
        << "  /status                  List submitted query statuses\n"
        << "  /wait                    Wait for all work and print results\n"
        << "  /help                    Show this help\n"
        << "  /quit                    Drain queued work and exit\n"
        << "\n"
        << "Backend selection: use --backend demo or --backend mysql.\n";
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
        if (!query.columns.empty()) {
            for (std::size_t column = 0; column < query.columns.size(); ++column) {
                std::cout << (column == 0 ? "" : " | ") << query.columns[column];
            }
            std::cout << '\n';
            for (const auto& row : query.rows) {
                for (std::size_t column = 0; column < row.size(); ++column) {
                    std::cout << (column == 0 ? "" : " | ") << row[column];
                }
                std::cout << '\n';
            }
        }
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

void printBatchSummary(const queryflow::BatchReportSummary& summary) {
    const auto printAverage = [](const char* label,
                                 const std::optional<double>& value) {
        std::cout << label << ": ";
        if (value.has_value()) {
            std::cout << *value << " ms\n";
        } else {
            std::cout << "N/A\n";
        }
    };

    std::cout << "\nBatch " << summary.batchId << " summary\n"
              << "  Statements discovered: " << summary.discovered << '\n'
              << "  Queries submitted: " << summary.submitted << '\n'
              << "  Completed: " << summary.completed << '\n'
              << "  Failed: " << summary.failed << '\n'
              << "  Elapsed: " << summary.elapsed.count() << " seconds\n";
    printAverage("  Average waiting", summary.averageWaitingMs);
    printAverage("  Average execution", summary.averageExecutionMs);
    printAverage("  Average response", summary.averageResponseMs);
    std::cout << "  Throughput: " << summary.throughputQueriesPerSecond
              << " completed queries/second\n"
              << "  Returned rows: "
              << (summary.returnedRows.has_value()
                      ? std::to_string(*summary.returnedRows)
                      : "N/A")
              << "\n  Log: " << summary.logPath.string()
              << "\n  CSV: " << summary.csvPath.string() << '\n';
}

}  // namespace

int main(int argc, char* argv[]) {
    std::size_t workerCount = 4;
    queryflow::SchedulingPolicy policy = queryflow::SchedulingPolicy::FCFS;
    std::string backend = "demo";
    std::string sqlFile;
    bool batchMode = false;
    std::string logDirectory = "logs";
    int batchPriority = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
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
            std::string policyName = argv[++i];
            for (char& character : policyName) {
                character = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(character)));
            }
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
        if (argument == "--backend") {
            if (i + 1 >= argc) {
                std::cerr << "--backend requires demo or mysql\n";
                return 2;
            }
            backend = argv[++i];
            if (backend != "demo" && backend != "mysql") {
                std::cerr << "Unknown backend '" << backend
                          << "'; choose demo or mysql\n";
                return 2;
            }
            continue;
        }
        if (argument == "--file") {
            if (i + 1 >= argc) {
                std::cerr << "--file requires a SQL workload path\n";
                return 2;
            }
            sqlFile = argv[++i];
            batchMode = true;
            continue;
        }
        if (argument == "--log-dir") {
            if (i + 1 >= argc) {
                std::cerr << "--log-dir requires a directory path\n";
                return 2;
            }
            logDirectory = argv[++i];
            continue;
        }
        if (argument == "--priority") {
            if (i + 1 >= argc || !parsePriority(argv[i + 1], batchPriority)) {
                std::cerr << "--priority requires an integer value\n";
                return 2;
            }
            ++i;
            continue;
        }
        if (argument == "--db-host") {
            if (i + 1 >= argc) {
                std::cerr << "--db-host requires a host string\n";
                return 2;
            }
            setenv("QUERYFLOW_DB_HOST", argv[++i], 1);
            continue;
        }
        if (argument == "--db-port") {
            if (i + 1 >= argc) {
                std::cerr << "--db-port requires an integer port\n";
                return 2;
            }
            setenv("QUERYFLOW_DB_PORT", argv[++i], 1);
            continue;
        }
        if (argument == "--db-name") {
            if (i + 1 >= argc) {
                std::cerr << "--db-name requires a schema name\n";
                return 2;
            }
            setenv("QUERYFLOW_DB_NAME", argv[++i], 1);
            continue;
        }
        if (argument == "--db-user") {
            if (i + 1 >= argc) {
                std::cerr << "--db-user requires a username\n";
                return 2;
            }
            setenv("QUERYFLOW_DB_USER", argv[++i], 1);
            continue;
        }
        if (argument == "--db-password") {
            if (i + 1 >= argc) {
                std::cerr << "--db-password requires a password string\n";
                return 2;
            }
            setenv("QUERYFLOW_DB_PASSWORD", argv[++i], 1);
            continue;
        }
        std::cerr << "Unknown option: " << argument << '\n';
        return 2;
    }

    std::vector<queryflow::SqlStatement> batchStatements;
    if (batchMode) {
        try {
            batchStatements = queryflow::readSqlStatements(sqlFile);
        } catch (const std::exception& exception) {
            std::cerr << "Could not read SQL batch: " << exception.what() << '\n';
            return 2;
        }
    }

#ifndef QUERYFLOW_WITH_MYSQL
    if (backend == "mysql") {
        std::cerr << "This build has no MySQL Connector/C++ support. Reconfigure with "
                     "-DQUERYFLOW_ENABLE_MYSQL=ON after installing the connector.\n";
        return 2;
    }
#endif

    std::unique_ptr<queryflow::IDatabaseManager> database;
    if (backend == "mysql") {
#ifdef QUERYFLOW_WITH_MYSQL
        auto mysqlDatabase = std::make_unique<queryflow::DatabaseManager>();
        try {
            if (!mysqlDatabase->ping()) {
                std::cerr << "MySQL connection check failed.\n";
                return 1;
            }
        } catch (const std::exception& exception) {
            std::cerr << "Could not connect to MySQL: " << exception.what() << '\n';
            return 1;
        }
        database = std::move(mysqlDatabase);
#endif
    } else {
        database = std::make_unique<queryflow::MockDatabaseManager>();
    }

    queryflow::QueryManager manager(workerCount, policy, *database);
    if (batchMode) {
        try {
            const auto summary = queryflow::runSqlStatements(
                batchStatements, manager, policy, logDirectory, batchPriority);
            for (const auto& failure : summary.failures) {
                std::cerr << failure << '\n';
            }
            printBatchSummary(summary);
            manager.shutdown();
            return summary.failed == 0 ? 0 : 1;
        } catch (const std::exception& exception) {
            manager.shutdown();
            std::cerr << "Batch execution/reporting failed: " << exception.what()
                      << '\n';
            return 1;
        }
    }

    std::set<int> reportedQueryIds;

    std::cout << "QueryFlow CLI (workers=" << workerCount << ", policy="
              << (policy == queryflow::SchedulingPolicy::FCFS ? "FCFS" : "priority")
              << ")\n"
              << "Backend: " << backend << '\n'
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
