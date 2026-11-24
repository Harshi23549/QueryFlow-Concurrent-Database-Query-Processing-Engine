#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "queryflow/core/Query.h"
#include "queryflow/scheduler/Scheduler.h"

namespace queryflow {

struct BatchStatementRecord {
    std::size_t statementNumber = 0;
    std::string sql;
    int priority = 0;
    std::optional<int> queryId;
    std::optional<QuerySnapshot> query;
    std::string submissionError;
};

struct BatchReportSummary {
    std::string batchId;
    std::filesystem::path logPath;
    std::filesystem::path csvPath;
    std::size_t discovered = 0;
    std::size_t submitted = 0;
    std::size_t completed = 0;
    std::size_t failed = 0;
    std::chrono::duration<double> elapsed{};
    std::optional<double> averageWaitingMs;
    std::optional<double> averageExecutionMs;
    std::optional<double> averageResponseMs;
    double throughputQueriesPerSecond = 0.0;
    std::optional<std::size_t> returnedRows;
    std::vector<std::string> failures;
};

std::string createBatchId();
BatchReportSummary writeBatchReports(
    const std::filesystem::path& logDirectory,
    std::string batchId,
    SchedulingPolicy policy,
    std::chrono::steady_clock::time_point batchStarted,
    std::chrono::steady_clock::time_point batchFinished,
    std::size_t discovered,
    std::size_t submitted,
    const std::vector<BatchStatementRecord>& records);

}  // namespace queryflow
