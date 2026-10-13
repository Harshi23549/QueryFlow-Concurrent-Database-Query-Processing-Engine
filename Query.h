#pragma once

#include <chrono>
#include <string>

// The unit of work passed from the Query Manager, through the shared queue,
// to a worker thread, and finally into the Database Manager.
struct Query {
    int id;
    std::string sql;
    std::chrono::steady_clock::time_point submitTime;

    Query() : id(0), submitTime(std::chrono::steady_clock::now()) {}

    Query(int id_, std::string sql_)
        : id(id_), sql(std::move(sql_)), submitTime(std::chrono::steady_clock::now()) {}
};
