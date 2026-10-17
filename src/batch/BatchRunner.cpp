#include "queryflow/batch/BatchRunner.h"

#include <chrono>
#include <map>
#include <stdexcept>
#include <utility>

#include "queryflow/batch/SqlFileReader.h"

namespace queryflow {

BatchReportSummary runSqlBatch(const std::filesystem::path& sqlFile,
                              QueryManager& manager,
                              SchedulingPolicy policy,
                              const std::filesystem::path& logDirectory,
                              int priority) {
    const auto statements = readSqlStatements(sqlFile);
    return runSqlStatements(statements, manager, policy, logDirectory, priority);
}

BatchReportSummary runSqlStatements(
    const std::vector<SqlStatement>& statements,
    QueryManager& manager,
    SchedulingPolicy policy,
    const std::filesystem::path& logDirectory,
    int priority) {
    if (statements.empty()) {
        throw std::invalid_argument("SQL batch contains no statements");
    }
    const std::string batchId = createBatchId();
    const auto batchStarted = std::chrono::steady_clock::now();

    std::vector<BatchStatementRecord> records;
    records.reserve(statements.size());
    std::map<int, std::size_t> statementByQueryId;
    std::size_t submitted = 0;

    for (const auto& statement : statements) {
        BatchStatementRecord record;
        record.statementNumber = statement.number;
        record.sql = statement.sql;
        record.priority = priority;
        try {
            const auto query = manager.submit(statement.sql, priority);
            record.queryId = query.id;
            statementByQueryId.emplace(query.id, statement.number);
            ++submitted;
        } catch (const std::exception& error) {
            record.submissionError = error.what();
        }
        records.push_back(std::move(record));
    }

    manager.waitUntilIdle();
    const auto batchFinished = std::chrono::steady_clock::now();
    const auto snapshots = manager.getQueries();
    std::map<int, QuerySnapshot> snapshotsById;
    for (const auto& snapshot : snapshots) {
        if (statementByQueryId.count(snapshot.id) != 0) {
            snapshotsById.emplace(snapshot.id, snapshot);
        }
    }

    for (auto& record : records) {
        if (!record.queryId.has_value()) {
            continue;
        }
        const auto found = snapshotsById.find(*record.queryId);
        if (found == snapshotsById.end()) {
            throw std::runtime_error(
                "Accepted query " + std::to_string(*record.queryId) +
                " has no lifecycle snapshot after the batch completed");
        }
        if (found->second.status != QueryStatus::Completed &&
            found->second.status != QueryStatus::Failed) {
            throw std::runtime_error(
                "Accepted query " + std::to_string(*record.queryId) +
                " did not reach a terminal state");
        }
        record.query = found->second;
    }

    auto summary = writeBatchReports(logDirectory,
                                     batchId,
                                     policy,
                                     batchStarted,
                                     batchFinished,
                                     statements.size(),
                                     submitted,
                                     records);
    return summary;
}

}  // namespace queryflow
