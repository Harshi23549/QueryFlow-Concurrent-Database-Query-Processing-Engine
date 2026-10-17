#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "queryflow/batch/BatchReport.h"
#include "queryflow/batch/SqlFileReader.h"
#include "queryflow/manager/QueryManager.h"

namespace queryflow {

BatchReportSummary runSqlStatements(const std::vector<SqlStatement>& statements,
                                   QueryManager& manager,
                                   SchedulingPolicy policy,
                                   const std::filesystem::path& logDirectory,
                                   int priority = 0);

BatchReportSummary runSqlBatch(const std::filesystem::path& sqlFile,
                              QueryManager& manager,
                              SchedulingPolicy policy,
                              const std::filesystem::path& logDirectory,
                              int priority = 0);

}  // namespace queryflow
