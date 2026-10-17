#include "queryflow/batch/BatchReport.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "queryflow/metrics/QueryMetrics.h"

namespace queryflow {
namespace {

std::string policyName(SchedulingPolicy policy) {
    return policy == SchedulingPolicy::FCFS ? "FCFS" : "PRIORITY";
}

std::string timestamp(std::chrono::system_clock::time_point time) {
    if (time.time_since_epoch().count() == 0) {
        return "N/A";
    }
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 time.time_since_epoch()) %
                             std::chrono::seconds(1);
    const std::time_t raw = std::chrono::system_clock::to_time_t(time);
    std::tm utc {};
    gmtime_r(&raw, &utc);
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
           << '.' << std::setw(3) << std::setfill('0') << milliseconds.count()
           << 'Z';
    return output.str();
}

bool isIdentifierCharacter(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || ch == '_' || ch == '$';
}

std::string redactSqlLiterals(const std::string& sql) {
    std::string output;
    output.reserve(sql.size());
    for (std::size_t i = 0; i < sql.size();) {
        const char quote = sql[i];
        if (quote == '`') {
            output.push_back(sql[i++]);
            while (i < sql.size()) {
                const char ch = sql[i++];
                output.push_back(ch);
                if (ch == '`') {
                    if (i < sql.size() && sql[i] == '`') {
                        output.push_back(sql[i++]);
                    } else {
                        break;
                    }
                } else if (ch == '\\' && i < sql.size()) {
                    output.push_back(sql[i++]);
                }
            }
            continue;
        }
        if (quote == '\'' || quote == '"') {
            output.push_back(quote);
            ++i;
            bool hasContent = false;
            while (i < sql.size()) {
                const char ch = sql[i];
                if (ch == '\\' && i + 1 < sql.size()) {
                    hasContent = true;
                    i += 2;
                    continue;
                }
                if (ch == quote) {
                    if (i + 1 < sql.size() && sql[i + 1] == quote) {
                        hasContent = true;
                        i += 2;
                        continue;
                    }
                    if (hasContent) {
                        output += "[REDACTED]";
                    }
                    output.push_back(quote);
                    ++i;
                    break;
                }
                hasContent = true;
                ++i;
            }
            continue;
        }

        const bool startsNumber =
            quote >= '0' && quote <= '9' &&
            (i == 0 || !isIdentifierCharacter(sql[i - 1]));
        if (startsNumber) {
            std::size_t end = i;
            bool exponent = false;
            while (end < sql.size()) {
                const char ch = sql[end];
                if ((ch >= '0' && ch <= '9') || ch == '.') {
                    ++end;
                } else if ((ch == 'e' || ch == 'E') && !exponent) {
                    exponent = true;
                    ++end;
                    if (end < sql.size() && (sql[end] == '+' || sql[end] == '-')) {
                        ++end;
                    }
                } else if ((ch == 'x' || ch == 'X') && end == i + 1 &&
                           sql[i] == '0' && end + 1 < sql.size()) {
                    ++end;
                    while (end < sql.size()) {
                        const char hex = sql[end];
                        if ((hex >= '0' && hex <= '9') ||
                            (hex >= 'a' && hex <= 'f') ||
                            (hex >= 'A' && hex <= 'F')) {
                            ++end;
                        } else {
                            break;
                        }
                    }
                    break;
                } else {
                    break;
                }
            }
            output += "[REDACTED]";
            i = end;
            continue;
        }

        output.push_back(sql[i++]);
    }
    return output;
}

std::string csv(const std::string& value) {
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string numberOrNA(const std::optional<double>& value) {
    if (!value.has_value()) {
        return "N/A";
    }
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << *value;
    return output.str();
}

std::optional<double> mean(const std::vector<double>& values) {
    if (values.empty()) {
        return std::nullopt;
    }
    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum / static_cast<double>(values.size());
}

}  // namespace

std::string createBatchId() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
    gmtime_r(&raw, &utc);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now.time_since_epoch()) %
                             std::chrono::seconds(1);
    std::ostringstream id;
    id << std::put_time(&utc, "%Y%m%dT%H%M%S")
       << '_' << std::setw(3) << std::setfill('0') << milliseconds.count();
    return id.str();
}

BatchReportSummary writeBatchReports(
    const std::filesystem::path& logDirectory,
    std::string batchId,
    SchedulingPolicy policy,
    std::chrono::steady_clock::time_point batchStarted,
    std::chrono::steady_clock::time_point batchFinished,
    std::size_t discovered,
    std::size_t submitted,
    const std::vector<BatchStatementRecord>& records) {
    std::error_code filesystemError;
    std::filesystem::create_directories(logDirectory, filesystemError);
    if (filesystemError) {
        throw std::runtime_error("Cannot create log directory '" +
                                 logDirectory.string() + "': " +
                                 filesystemError.message());
    }

    const auto elapsed = std::chrono::duration<double>(batchFinished - batchStarted);
    std::filesystem::path logPath;
    std::filesystem::path csvPath;
    std::filesystem::path reservationPath;
    for (std::size_t suffix = 0;; ++suffix) {
        const std::string uniqueId =
            batchId + (suffix == 0 ? "" : "_" + std::to_string(suffix));
        logPath = logDirectory / ("queryflow-batch-" + uniqueId + ".log");
        csvPath = logDirectory / ("queryflow-batch-" + uniqueId + ".csv");
        reservationPath = logDirectory / (".queryflow-batch-" + uniqueId + ".lock");
        if (std::filesystem::exists(logPath) || std::filesystem::exists(csvPath)) {
            continue;
        }
        filesystemError.clear();
        if (std::filesystem::create_directory(reservationPath, filesystemError)) {
            batchId = uniqueId;
            break;
        }
        if (filesystemError && filesystemError != std::errc::file_exists) {
            throw std::runtime_error("Cannot reserve unique batch report name: " +
                                     filesystemError.message());
        }
    }

    std::ofstream csvFile(csvPath, std::ios::binary | std::ios::out);
    std::ofstream logFile(logPath, std::ios::out);
    if (!csvFile || !logFile) {
        csvFile.close();
        logFile.close();
        std::filesystem::remove(logPath, filesystemError);
        filesystemError.clear();
        std::filesystem::remove(csvPath, filesystemError);
        filesystemError.clear();
        std::filesystem::remove_all(reservationPath, filesystemError);
        throw std::runtime_error("Cannot create batch reports in '" +
                                 logDirectory.string() + "'");
    }

    BatchReportSummary summary;
    summary.batchId = batchId;
    summary.logPath = logPath;
    summary.csvPath = csvPath;
    summary.discovered = discovered;
    summary.submitted = submitted;
    summary.elapsed = elapsed;

    std::vector<double> waitingTimes;
    std::vector<double> executionTimes;
    std::vector<double> responseTimes;
    std::size_t totalRows = 0;
    bool hasRowCounts = false;

    csvFile << "batch_id,statement_number,query_id,sql,priority,policy,worker_id,"
               "submitted_at,started_at,completed_at,status,waiting_ms,execution_ms,"
               "response_ms,returned_rows,affected_rows,error\n";
    logFile << "QueryFlow batch " << batchId << "\n"
            << "Scheduling policy: " << policyName(policy) << "\n"
            << "Statements discovered: " << discovered << "\n"
            << "Queries submitted: " << submitted << "\n\n";

    for (const auto& record : records) {
        const QuerySnapshot* query = record.query ? &*record.query : nullptr;
        std::string status = "failed";
        std::string worker = "N/A";
        std::string submittedAt = "N/A";
        std::string startedAt = "N/A";
        std::string finishedAt = "N/A";
        std::string waiting = "N/A";
        std::string execution = "N/A";
        std::string response = "N/A";
        std::string rows = "N/A";
        std::string affectedRows = "N/A";
        std::string error = redactSqlLiterals(record.submissionError);

        if (query != nullptr) {
            status = toString(query->status);
            worker = query->workerId.empty() ? "N/A" : query->workerId;
            submittedAt = timestamp(query->submittedAt);
            startedAt = timestamp(query->startedAt);
            finishedAt = timestamp(query->finishedAt);
            error = redactSqlLiterals(query->error);

            if (query->status == QueryStatus::Completed) {
                ++summary.completed;
                if (!query->columns.empty()) {
                    rows = std::to_string(query->rows.size());
                    totalRows += query->rows.size();
                    hasRowCounts = true;
                }
                if (query->affectedRowsAvailable) {
                    affectedRows = std::to_string(query->affectedRows);
                }

                const auto metrics = QueryMetrics::fromSnapshot(*query);
                const double waitingMs = metrics.waitingTimeMs();
                const double executionMs = metrics.executionTimeMs();
                const double responseMs = metrics.responseTimeMs();
                waitingTimes.push_back(waitingMs);
                executionTimes.push_back(executionMs);
                responseTimes.push_back(responseMs);
                waiting = numberOrNA(waitingMs);
                execution = numberOrNA(executionMs);
                response = numberOrNA(responseMs);
            } else if (query->status == QueryStatus::Failed) {
                ++summary.failed;
                summary.failures.push_back(
                    "Statement #" + std::to_string(record.statementNumber) +
                    " (Q" + std::to_string(query->id) + "): " + query->error);
            }
        } else {
            ++summary.failed;
            summary.failures.push_back(
                "Statement #" + std::to_string(record.statementNumber) +
                " submission failed: " + record.submissionError);
        }

        const std::string safeSql = redactSqlLiterals(record.sql);
        csvFile << csv(batchId) << ',' << record.statementNumber << ','
                << (record.queryId.has_value()
                        ? std::to_string(*record.queryId)
                        : "N/A")
                << ',' << csv(safeSql) << ',' << record.priority << ','
                << csv(policyName(policy)) << ',' << csv(worker) << ','
                << csv(submittedAt) << ',' << csv(startedAt) << ','
                << csv(finishedAt) << ',' << csv(status) << ',' << csv(waiting)
                << ',' << csv(execution) << ',' << csv(response) << ','
                << csv(rows) << ',' << csv(affectedRows) << ',' << csv(error) << '\n';

        logFile << "Statement #" << record.statementNumber << "\n"
                << "  Query ID: "
                << (record.queryId.has_value() ? std::to_string(*record.queryId) : "N/A")
                << "\n  SQL (literal values redacted): " << safeSql
                << "\n  Priority: " << record.priority
                << "\n  Policy: " << policyName(policy)
                << "\n  Worker: " << worker
                << "\n  Submitted: " << submittedAt
                << "\n  Started: " << startedAt
                << "\n  Completed: " << finishedAt
                << "\n  Status: " << status
                << "\n  Waiting ms: " << waiting
                << "\n  Execution ms: " << execution
                << "\n  Response ms: " << response
                << "\n  Returned rows: " << rows
                << "\n  Affected rows: " << affectedRows
                << "\n  Error: " << (error.empty() ? "N/A" : error) << "\n\n";
    }

    summary.averageWaitingMs = mean(waitingTimes);
    summary.averageExecutionMs = mean(executionTimes);
    summary.averageResponseMs = mean(responseTimes);
    summary.returnedRows = hasRowCounts ? std::optional<std::size_t>(totalRows)
                                        : std::nullopt;
    if (elapsed.count() > 0.0) {
        summary.throughputQueriesPerSecond =
            static_cast<double>(summary.completed) / elapsed.count();
    }

    logFile << "Batch summary\n"
            << "  Discovered: " << summary.discovered
            << "\n  Submitted: " << summary.submitted
            << "\n  Completed: " << summary.completed
            << "\n  Failed: " << summary.failed
            << "\n  Elapsed seconds: " << std::fixed << std::setprecision(6)
            << summary.elapsed.count()
            << "\n  Average waiting ms: " << numberOrNA(summary.averageWaitingMs)
            << "\n  Average execution ms: " << numberOrNA(summary.averageExecutionMs)
            << "\n  Average response ms: " << numberOrNA(summary.averageResponseMs)
            << "\n  Throughput completed queries/elapsed seconds: "
            << std::fixed << std::setprecision(3)
            << summary.throughputQueriesPerSecond
            << "\n  Total returned rows: "
            << (summary.returnedRows.has_value()
                    ? std::to_string(*summary.returnedRows)
                    : "N/A")
            << '\n';

    csvFile.flush();
    logFile.flush();
    if (!csvFile || !logFile) {
        csvFile.close();
        logFile.close();
        std::filesystem::remove(logPath, filesystemError);
        filesystemError.clear();
        std::filesystem::remove(csvPath, filesystemError);
        filesystemError.clear();
        std::filesystem::remove_all(reservationPath, filesystemError);
        throw std::runtime_error("Failed while writing batch reports");
    }
    csvFile.close();
    logFile.close();
    std::filesystem::remove_all(reservationPath, filesystemError);
    if (filesystemError) {
        throw std::runtime_error("Could not release batch report reservation: " +
                                 filesystemError.message());
    }
    return summary;
}

}  // namespace queryflow
