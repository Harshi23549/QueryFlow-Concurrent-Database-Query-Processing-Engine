#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "queryflow/batch/BatchRunner.h"
#include "queryflow/batch/SqlFileReader.h"
#include "queryflow/database/IDatabaseManager.h"
#include "queryflow/manager/QueryManager.h"

namespace {

class TestDatabase : public queryflow::IDatabaseManager {
public:
    queryflow::DatabaseResult execute(const queryflow::Query& query) override {
        if (query.sql.find("FAIL") != std::string::npos) {
            throw std::runtime_error("synthetic \"failure\",\nwith comma");
        }
        queryflow::DatabaseResult result;
        result.columns = {"value"};
        result.rows = {{query.sql.find("SELECT 2") != std::string::npos ? "2" : "1"}};
        return result;
    }
};

std::filesystem::path makeTempDirectory() {
    const auto path = std::filesystem::temp_directory_path() /
                      ("queryflow_batch_test_" + queryflow::createBatchId());
    std::filesystem::create_directories(path);
    return path;
}

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
    if (!file) {
        throw std::runtime_error("test could not write a temporary SQL file");
    }
}

}  // namespace

int main() {
    const auto tempDirectory = makeTempDirectory();
    const auto workloadPath = tempDirectory / "input.sql";
    writeFile(workloadPath,
              "-- leading comment with ;\n"
              "SELECT 'a;comma,b', \"x;y\"; /* block ; comment */\n"
              "\n"
              "SELECT\n"
              "  'it''s fine' AS text_value\n"
              "FROM customers; -- trailing comment\n"
              "SELECT 3");

    const auto statements = queryflow::readSqlStatements(workloadPath);
    assert(statements.size() == 3);
    assert(statements[0].number == 1);
    assert(statements[0].sql.find("'a;comma,b'") != std::string::npos);
    assert(statements[1].sql.find("FROM customers") != std::string::npos);
    assert(statements[2].sql == "SELECT 3");

    const auto emptyPath = tempDirectory / "empty.sql";
    writeFile(emptyPath, " -- only a comment\n/* and another */\n");
    bool rejectedEmpty = false;
    try {
        (void)queryflow::readSqlStatements(emptyPath);
    } catch (const std::runtime_error&) {
        rejectedEmpty = true;
    }
    assert(rejectedEmpty);

    const auto malformedPath = tempDirectory / "malformed.sql";
    writeFile(malformedPath, "SELECT 'unterminated");
    bool rejectedMalformed = false;
    try {
        (void)queryflow::readSqlStatements(malformedPath);
    } catch (const std::runtime_error&) {
        rejectedMalformed = true;
    }
    assert(rejectedMalformed);

    bool rejectedMissingFile = false;
    try {
        (void)queryflow::readSqlStatements(tempDirectory / "missing.sql");
    } catch (const std::runtime_error&) {
        rejectedMissingFile = true;
    }
    assert(rejectedMissingFile);

    const auto batchInput = tempDirectory / "batch.sql";
    writeFile(batchInput,
              "SELECT 'secret, value';\n"
              "SELECT 2;\n"
              "SELECT FAIL;");

    TestDatabase database;
    queryflow::QueryManager manager(
        2, queryflow::SchedulingPolicy::FCFS, database);
    const auto summary = queryflow::runSqlBatch(
        batchInput, manager, queryflow::SchedulingPolicy::FCFS, tempDirectory);

    assert(summary.discovered == 3);
    assert(summary.submitted == 3);
    assert(summary.completed == 2);
    assert(summary.failed == 1);
    assert(summary.averageWaitingMs.has_value());
    assert(summary.averageExecutionMs.has_value());
    assert(summary.averageResponseMs.has_value());
    assert(summary.returnedRows == 2);
    assert(summary.throughputQueriesPerSecond > 0.0);
    assert(std::filesystem::exists(summary.logPath));
    assert(std::filesystem::exists(summary.csvPath));

    std::ifstream csvFile(summary.csvPath);
    const std::string csvText{
        std::istreambuf_iterator<char>(csvFile), std::istreambuf_iterator<char>()};
    assert(csvText.find("statement_number,query_id,sql") != std::string::npos);
    assert(csvText.find("'[REDACTED]'") != std::string::npos);
    assert(csvText.find("synthetic \"\"[REDACTED]\"\",\nwith comma") !=
           std::string::npos);
    assert(csvText.find("\"synthetic \"\"[REDACTED]\"\",\nwith comma\"") !=
           std::string::npos);
    assert(csvText.find("SELECT '[REDACTED]'") != std::string::npos);
    assert(csvText.find("worker-") != std::string::npos);

    std::ifstream logFile(summary.logPath);
    const std::string logText{
        std::istreambuf_iterator<char>(logFile), std::istreambuf_iterator<char>()};
    assert(logText.find("Batch summary") != std::string::npos);
    assert(logText.find("worker-") != std::string::npos);
    assert(logText.find("Average execution ms") != std::string::npos);

    const auto querySnapshots = manager.getQueries();
    assert(querySnapshots.size() == 3);
    assert(querySnapshots[0].id == 1);
    assert(querySnapshots[0].sql == "SELECT 'secret, value'");
    assert(querySnapshots[0].rows.front().front() == "1");
    assert(querySnapshots[1].id == 2);
    assert(querySnapshots[1].rows.front().front() == "2");
    assert(querySnapshots[2].id == 3);
    assert(querySnapshots[2].status == queryflow::QueryStatus::Failed);
    manager.shutdown();

    const auto noRowsPath = tempDirectory / "empty-result.sql";
    writeFile(noRowsPath, "SELECT 1");
    queryflow::QueryManager secondManager(
        1, queryflow::SchedulingPolicy::FCFS, database);
    const auto secondSummary = queryflow::runSqlBatch(
        noRowsPath, secondManager, queryflow::SchedulingPolicy::FCFS, tempDirectory);
    assert(secondSummary.completed == 1);
    assert(secondSummary.logPath != summary.logPath);
    assert(secondSummary.csvPath != summary.csvPath);

    std::filesystem::remove_all(tempDirectory);
    std::cout << "Batch parser, execution, and report tests passed!\n";
    return 0;
}
