#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace queryflow::db {

struct ConnectionSettings {
    std::string host = "127.0.0.1";
    unsigned int port = 3306;
    std::string schema = "queryflow";
    std::string user;
    std::string password;

    static ConnectionSettings fromEnvironment();
};

struct QueryResult {
    std::vector<std::string> columns;
    // A disengaged optional represents SQL NULL; an empty string remains distinct.
    std::vector<std::vector<std::optional<std::string>>> rows;
    unsigned long long affectedRows = 0;
};

class Transaction;

class Database {
public:
    explicit Database(const ConnectionSettings& settings);
    ~Database();
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Use execute only for trusted SQL with no untrusted values.
    QueryResult execute(const std::string& sql);
    // Bind values separately to prevent SQL injection. Values are sent as strings or NULL.
    QueryResult executePrepared(const std::string& sql,
                                const std::vector<std::optional<std::string>>& parameters);
    bool ping();
    Transaction beginTransaction();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend class Transaction;
};

class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();
    void rollback();
    bool active() const noexcept { return active_; }

private:
    void rollbackNoThrow() noexcept;
    Database* database_ = nullptr;
    bool active_ = false;
};

} // namespace queryflow::db
