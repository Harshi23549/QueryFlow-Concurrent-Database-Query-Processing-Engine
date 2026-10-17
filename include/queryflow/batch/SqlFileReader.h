#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace queryflow {

struct SqlStatement {
    std::size_t number = 0;
    std::string sql;
};

std::vector<SqlStatement> readSqlStatements(const std::filesystem::path& path);

}  // namespace queryflow
