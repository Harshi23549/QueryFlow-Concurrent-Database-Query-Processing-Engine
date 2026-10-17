#include "Database.hpp"

#if __has_include(<mysql/jdbc.h>)
#include <mysql/jdbc.h>
#else
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/datatype.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>
#endif

#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace queryflow::db {
namespace {
sql::Driver* connectorDriver() {
    // The classic API's singleton accessor is not safe for simultaneous first calls.
    static std::once_flag initialized;
    static sql::Driver* driver = nullptr;
    std::call_once(initialized, [] { driver = sql::mysql::get_driver_instance(); });
    return driver;
}

std::string trimEnv(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n'\"");
    if (first == std::string::npos) return "";
    const auto last = str.find_last_not_of(" \t\r\n'\"");
    return str.substr(first, last - first + 1);
}

std::map<std::string, std::string> loadDotEnv() {
    std::map<std::string, std::string> env;
    const std::vector<std::string> paths = {".env", "../.env", "../../.env"};
    for (const auto& path : paths) {
        std::ifstream file(path);
        if (!file.is_open()) continue;
        std::string line;
        while (std::getline(file, line)) {
            const auto commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            const auto eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            const auto key = trimEnv(line.substr(0, eqPos));
            const auto val = trimEnv(line.substr(eqPos + 1));
            if (!key.empty()) {
                env[key] = val;
            }
        }
        break;
    }
    return env;
}

std::string environmentValue(const char* key, const char* fallback) {
    const char* value = std::getenv(key);
    if (value != nullptr && *value != '\0') {
        return value;
    }
    static const auto dotEnv = loadDotEnv();
    const auto it = dotEnv.find(key);
    if (it != dotEnv.end() && !it->second.empty()) {
        return it->second;
    }
    return fallback;
}


QueryResult collectRows(sql::ResultSet& rows) {
    QueryResult result;
    const auto metadata = rows.getMetaData();
    const auto count = metadata->getColumnCount();
    result.columns.reserve(count);
    for (unsigned int column = 1; column <= count; ++column) {
        result.columns.push_back(metadata->getColumnLabel(column));
    }
    while (rows.next()) {
        std::vector<std::optional<std::string>> row;
        row.reserve(count);
        for (unsigned int column = 1; column <= count; ++column) {
            const auto value = rows.getString(column);
            if (rows.wasNull()) {
                row.emplace_back(std::nullopt);
            } else {
                row.emplace_back(value);
            }
        }
        result.rows.push_back(std::move(row));
    }
    return result;
}
} // namespace

struct Database::Impl {
    sql::Driver* driver = nullptr; // Connector owns the singleton.
    std::unique_ptr<sql::Connection> connection;
};

ConnectionSettings ConnectionSettings::fromEnvironment() {
    const auto portText = environmentValue("QUERYFLOW_DB_PORT", "3306");
    char* end = nullptr;
    const auto parsedPort = std::strtoul(portText.c_str(), &end, 10);
    if (end == portText.c_str() || *end != '\0' || parsedPort == 0 || parsedPort > 65535) {
        throw std::invalid_argument("QUERYFLOW_DB_PORT must be an integer from 1 to 65535");
    }

    ConnectionSettings settings;
    settings.host = environmentValue("QUERYFLOW_DB_HOST", "127.0.0.1");
    settings.port = static_cast<unsigned int>(parsedPort);
    settings.schema = environmentValue("QUERYFLOW_DB_NAME", "queryflow");
    settings.user = environmentValue("QUERYFLOW_DB_USER", "queryflow_app");
    settings.password = environmentValue("QUERYFLOW_DB_PASSWORD", "");
    if (settings.user.empty()) {
        throw std::invalid_argument("QUERYFLOW_DB_USER must be set");
    }
    return settings;
}

Database::Database(const ConnectionSettings& settings) : impl_(std::make_unique<Impl>()) {
    impl_->driver = connectorDriver();
    const auto url = "tcp://" + settings.host + ":" + std::to_string(settings.port);
    impl_->connection.reset(impl_->driver->connect(url, settings.user, settings.password));
    impl_->connection->setSchema(settings.schema);
    impl_->connection->setAutoCommit(true);
}

Database::~Database() = default;
Database::Database(Database&&) noexcept = default;
Database& Database::operator=(Database&&) noexcept = default;

QueryResult Database::execute(const std::string& sqlText) {
    if (!impl_ || !impl_->connection) {
        throw std::runtime_error("Database connection is not available");
    }
    std::unique_ptr<sql::Statement> statement(impl_->connection->createStatement());
    QueryResult result;
    if (statement->execute(sqlText)) {
        std::unique_ptr<sql::ResultSet> rows(statement->getResultSet());
        result = collectRows(*rows);
    } else {
        result.affectedRows = statement->getUpdateCount();
    }
    return result;
}

QueryResult Database::executePrepared(
    const std::string& sqlText,
    const std::vector<std::optional<std::string>>& parameters) {
    if (!impl_ || !impl_->connection) {
        throw std::runtime_error("Database connection is not available");
    }
    std::unique_ptr<sql::PreparedStatement> statement(
        impl_->connection->prepareStatement(sqlText));
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (parameters[index].has_value()) {
            statement->setString(static_cast<unsigned int>(index + 1), *parameters[index]);
        } else {
            statement->setNull(static_cast<unsigned int>(index + 1), sql::DataType::VARCHAR);
        }
    }

    
    QueryResult result;
    const bool hasRows = statement->execute();

    if (hasRows) {
        std::unique_ptr<sql::ResultSet> rows(statement->getResultSet());
        if (rows) {
            result = collectRows(*rows);
        }
    } else {
        try {
            const auto count = statement->getUpdateCount();
            result.affectedRows = static_cast<std::size_t>(count);
        } catch (const sql::SQLException&) {

            // Some Connector/C++ versions may not expose the update count.
            result.affectedRows = 0;
        }
    }

    return result;
    }

bool Database::ping() {
    const auto result = execute("SELECT 1 AS connection_ok");
    return result.rows.size() == 1 && !result.rows.front().empty() &&
           result.rows.front().front().has_value() &&
           *result.rows.front().front() == "1";
}

Transaction Database::beginTransaction() { return Transaction(*this); }

Transaction::Transaction(Database& database) : database_(&database), active_(true) {
    if (!database.impl_ || !database.impl_->connection) {
        throw std::runtime_error("Database connection is not available");
    }
    database.impl_->connection->setAutoCommit(false);
}

Transaction::~Transaction() { rollbackNoThrow(); }

Transaction::Transaction(Transaction&& other) noexcept
    : database_(other.database_), active_(other.active_) {
    other.database_ = nullptr;
    other.active_ = false;
}

Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        rollbackNoThrow();
        database_ = other.database_;
        active_ = other.active_;
        other.database_ = nullptr;
        other.active_ = false;
    }
    return *this;
}

void Transaction::rollbackNoThrow() noexcept {
    if (!active_ || database_ == nullptr || !database_->impl_ ||
        !database_->impl_->connection) {
        return;
    }
    try {
        database_->impl_->connection->rollback();
        database_->impl_->connection->setAutoCommit(true);
    } catch (...) {
        // Destructors and move assignment must not throw.
    }
    active_ = false;
}

void Transaction::commit() {
    if (!active_ || database_ == nullptr) {
        throw std::logic_error("Transaction is no longer active");
    }
    database_->impl_->connection->commit();
    database_->impl_->connection->setAutoCommit(true);
    active_ = false;
}

void Transaction::rollback() {
    if (!active_ || database_ == nullptr) {
        throw std::logic_error("Transaction is no longer active");
    }
    database_->impl_->connection->rollback();
    database_->impl_->connection->setAutoCommit(true);
    active_ = false;
}

} // namespace queryflow::db
