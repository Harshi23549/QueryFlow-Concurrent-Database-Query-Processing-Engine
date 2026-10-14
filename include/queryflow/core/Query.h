#pragma once

#include <chrono>
#include <string>
#include <utility>

namespace queryflow {

struct Query {
    int id = 0;
    std::string sql;
    std::chrono::steady_clock::time_point submitTime;

    Query() : submitTime(std::chrono::steady_clock::now()) {}

    Query(int id_, std::string sql_)
        : id(id_), sql(std::move(sql_)), submitTime(std::chrono::steady_clock::now()) {}
};

}  // namespace queryflow
