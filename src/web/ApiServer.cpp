#include "queryflow/web/ApiServer.h"

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

#include "queryflow/manager/QueryManager.h"
#include "queryflow/metrics/QueryMetrics.h"
#include "queryflow/security/PasswordHash.h"

namespace queryflow {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using Tcp = asio::ip::tcp;
using Json = nlohmann::json;

std::int64_t epochMilliseconds(std::chrono::system_clock::time_point value) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               value.time_since_epoch())
        .count();
}

std::string policyName(SchedulingPolicy policy) {
    return policy == SchedulingPolicy::PRIORITY ? "priority" : "fcfs";
}

std::optional<double> average(const std::vector<double>& values) {
    if (values.empty()) {
        return std::nullopt;
    }
    double sum = 0.0;
    for (const auto value : values) {
        sum += value;
    }
    return sum / static_cast<double>(values.size());
}

void setOptional(Json& output, const char* key, const std::optional<double>& value) {
    output[key] = value.has_value() ? Json(*value) : Json(nullptr);
}

Json queryJson(const QuerySnapshot& query) {
    const auto now = std::chrono::steady_clock::now();
    Json output = {
        {"id", query.id},
        {"arrivalSequence", query.arrivalSequence},
        {"sql", query.sql},
        {"userId", query.userId},
        {"sessionLabel", query.sessionLabel},
        {"operation", query.operation},
        {"priority", query.priority},
        {"status", toString(query.status)},
        {"submittedAt", epochMilliseconds(query.submittedAt)},
        {"workerId", query.workerId.empty() ? Json(nullptr) : Json(query.workerId)},
        {"affectedRows", query.affectedRowsAvailable
                              ? Json(query.affectedRows)
                              : Json(nullptr)},
        {"error", query.error}
    };

    if (query.status == QueryStatus::Queued) {
        output["waitingMs"] = std::chrono::duration<double, std::milli>(
                                  now - query.submitTime)
                                  .count();
        output["executionMs"] = nullptr;
        output["responseMs"] = std::chrono::duration<double, std::milli>(
                                   now - query.submitTime)
                                   .count();
    } else if (query.status == QueryStatus::Running) {
        output["waitingMs"] = std::chrono::duration<double, std::milli>(
                                  query.startTime - query.submitTime)
                                  .count();
        output["executionMs"] = std::chrono::duration<double, std::milli>(
                                     now - query.startTime)
                                     .count();
        output["responseMs"] = std::chrono::duration<double, std::milli>(
                                   now - query.submitTime)
                                   .count();
    } else {
        const auto metrics = QueryMetrics::fromSnapshot(query);
        output["waitingMs"] = metrics.waitingTimeMs();
        output["executionMs"] = metrics.executionTimeMs();
        output["responseMs"] = metrics.responseTimeMs();
    }
    output["startedAt"] = query.startedAt.time_since_epoch().count() == 0
                               ? Json(nullptr)
                               : Json(epochMilliseconds(query.startedAt));
    output["finishedAt"] = query.finishedAt.time_since_epoch().count() == 0
                                ? Json(nullptr)
                                : Json(epochMilliseconds(query.finishedAt));
    return output;
}

Json eventJson(const QueryEvent& event) {
    return {
        {"eventId", event.eventId},
        {"timestamp", epochMilliseconds(event.timestamp)},
        {"type", event.type},
        {"queryId", event.queryId},
        {"userId", event.userId},
        {"workerId", event.workerId.empty() ? Json(nullptr) : Json(event.workerId)},
        {"detail", event.detail}
    };
}

Json workerJson(const WorkerSnapshot& worker) {
    Json output = {
        {"workerId", worker.workerId},
        {"state", worker.busy ? "busy" : "idle"},
        {"queryId", worker.busy ? Json(worker.queryId) : Json(nullptr)},
        {"operation", worker.busy ? Json(worker.operation) : Json(nullptr)},
        {"startedAt", worker.busy ? Json(epochMilliseconds(worker.startedAt))
                                   : Json(nullptr)},
        {"completedQueries", worker.completedQueries}
    };
    if (worker.busy) {
        output["executionMs"] = std::chrono::duration<double, std::milli>(
                                    std::chrono::system_clock::now() - worker.startedAt)
                                    .count();
    } else {
        output["executionMs"] = nullptr;
    }
    return output;
}

std::string boundedString(const Json& body,
                          const char* key,
                          const std::string& fallback,
                          std::size_t maximumLength) {
    if (!body.contains(key)) {
        return fallback;
    }
    if (!body[key].is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    auto value = body[key].get<std::string>();
    if (value.empty() || value.size() > maximumLength ||
        std::any_of(value.begin(), value.end(), [](unsigned char character) {
            return character < 0x20;
        })) {
        throw std::invalid_argument(std::string(key) + " has an invalid value");
    }
    return value;
}

std::size_t parseAfter(const std::string& target) {
    const auto marker = target.find("after=");
    if (marker == std::string::npos) {
        return 0;
    }
    const auto first = target.data() + marker + 6;
    const auto last = target.data() + target.size();
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(first, last, value);
    return parsed.ec == std::errc{} ? static_cast<std::size_t>(value) : 0;
}

class ApiError : public std::runtime_error {
public:
    ApiError(http::status status, const std::string& message)
        : std::runtime_error(message), status_(status) {}

    http::status status() const { return status_; }

private:
    http::status status_;
};

http::status errorStatus(const std::exception& exception) {
    if (const auto* error = dynamic_cast<const ApiError*>(&exception)) {
        return error->status();
    }
    if (dynamic_cast<const Json::exception*>(&exception) != nullptr) {
        return http::status::bad_request;
    }
    return dynamic_cast<const std::invalid_argument*>(&exception) != nullptr
               ? http::status::bad_request
               : http::status::internal_server_error;
}

}  // namespace

struct ApiServer::Impl {
    Impl(QueryManager& queryManager,
         std::string activeBackend,
         std::string publicHost,
         unsigned short storefrontListenPort,
         unsigned short adminListenPort)
        : manager(queryManager),
          backend(std::move(activeBackend)),
          storefrontHost(std::move(publicHost)),
          storefrontPort(storefrontListenPort),
          adminPort(adminListenPort),
          storefrontAcceptor(context),
          adminAcceptor(context),
          handlers(4) {}

    QueryManager& manager;
    std::string backend;
    std::string storefrontHost;
    unsigned short storefrontPort;
    unsigned short adminPort;
    asio::io_context context{1};
    Tcp::acceptor storefrontAcceptor;
    Tcp::acceptor adminAcceptor;
    asio::thread_pool handlers;
    std::mutex submissionMutex;
    std::mutex sessionsMutex;
    std::unordered_map<std::string, std::uint64_t> sessions;

    QuerySnapshot executeQuery(
        const std::string& sql,
        std::vector<std::optional<std::string>> parameters,
        int priority,
        const std::string& customerId,
        const std::string& operation) {
        const auto accepted = manager.submitPrepared(sql,
                                                      std::move(parameters),
                                                      priority,
                                                      customerId,
                                                      "storefront",
                                                      operation);
        manager.waitUntilIdle();
        const auto snapshot = manager.getQuery(accepted.id);
        if (!snapshot.has_value()) {
            throw std::runtime_error("QueryFlow lost the query result");
        }
        if (snapshot->status != QueryStatus::Completed) {
            throw std::runtime_error(snapshot->error.empty()
                                         ? "QueryFlow could not complete the request"
                                         : snapshot->error);
        }
        return *snapshot;
    }

    std::uint64_t customerFor(const http::request<http::string_body>& request) {
        const auto header = request.find("X-QueryFlow-Session");
        if (header == request.end()) {
            throw ApiError(http::status::unauthorized, "Please sign in to continue");
        }
        const auto value = header->value();
        const std::string token(value.data(), value.size());
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto found = sessions.find(token);
        if (found == sessions.end()) {
            throw ApiError(http::status::unauthorized, "Your session expired; sign in again");
        }
        return found->second;
    }

    Json accountJson(std::uint64_t customerId) {
        const auto query = executeQuery(
            "SELECT customer_id, full_name, email FROM customers WHERE customer_id = ?",
            {std::to_string(customerId)}, 20, std::to_string(customerId),
            "account-read");
        if (query.rows.size() != 1 || query.rows.front().size() != 3) {
            throw ApiError(http::status::unauthorized, "Customer account was not found");
        }
        return {{"customerId", query.rows.front()[0]},
                {"fullName", query.rows.front()[1]},
                {"email", query.rows.front()[2]}};
    }

    Json rowObjects(const QuerySnapshot& query) const {
        Json output = Json::array();
        for (const auto& row : query.rows) {
            Json item = Json::object();
            for (std::size_t column = 0;
                 column < query.columns.size() && column < row.size();
                 ++column) {
                item[query.columns[column]] = row[column];
            }
            output.push_back(std::move(item));
        }
        return output;
    }

    std::pair<std::string, std::string> registerCustomer(const Json& body) {
        const auto fullName = boundedString(body, "fullName", "", 120);
        auto email = boundedString(body, "email", "", 254);
        const auto password = boundedString(body, "password", "", 256);
        std::transform(email.begin(), email.end(), email.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        const auto at = email.find('@');
        if (at == std::string::npos || at == 0 || at + 1 >= email.size() ||
            email.find('@', at + 1) != std::string::npos) {
            throw std::invalid_argument("Enter a valid email address");
        }
        if (password.size() < 10) {
            throw std::invalid_argument("Password must contain at least 10 characters");
        }

        std::lock_guard<std::mutex> lock(submissionMutex);
        const auto existing = executeQuery(
            "SELECT customer_id FROM customers WHERE email = ?",
            {email}, 40, {}, "account-register-check");
        if (!existing.rows.empty()) {
            throw ApiError(http::status::conflict,
                           "An account with this email already exists");
        }

        const auto [passwordHash, salt] = hashPassword(password);
        const auto created = executeQuery(
            "REGISTER", {fullName, email, passwordHash, salt},
            50, {}, "account-register");
        if (created.rows.size() != 1 || created.rows.front().empty()) {
            throw std::runtime_error("Could not load the created customer account");
        }
        const auto customerId = created.rows.front().front();
        const auto token = createSessionToken();
        {
            std::lock_guard<std::mutex> sessionLock(sessionsMutex);
            sessions.emplace(token, std::stoull(customerId));
        }
        return {customerId, token};
    }

    std::pair<std::string, std::string> signIn(const Json& body) {
        auto email = boundedString(body, "email", "", 254);
        const auto password = boundedString(body, "password", "", 256);
        std::transform(email.begin(), email.end(), email.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        const auto customer = executeQuery(
            "AUTHENTICATE", {email, password}, 40, {}, "account-login");
        if (customer.rows.size() != 1 || customer.rows.front().size() != 3) {
            throw ApiError(http::status::unauthorized, "Email or password is incorrect");
        }
        const auto token = createSessionToken();
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            sessions.emplace(token, std::stoull(customer.rows.front()[0]));
        }
        return {customer.rows.front()[0], token};
    }

    void run() {
        const Tcp::endpoint storefrontEndpoint(
            asio::ip::make_address(storefrontHost), storefrontPort);
        storefrontAcceptor.open(storefrontEndpoint.protocol());
        storefrontAcceptor.set_option(Tcp::acceptor::reuse_address(true));
        storefrontAcceptor.bind(storefrontEndpoint);
        storefrontAcceptor.listen(asio::socket_base::max_listen_connections);

        const Tcp::endpoint adminEndpoint(
            asio::ip::make_address("127.0.0.1"), adminPort);
        adminAcceptor.open(adminEndpoint.protocol());
        adminAcceptor.set_option(Tcp::acceptor::reuse_address(true));
        adminAcceptor.bind(adminEndpoint);
        adminAcceptor.listen(asio::socket_base::max_listen_connections);

        asio::signal_set signals(context, SIGINT, SIGTERM);
        signals.async_wait([this](const boost::system::error_code& error, int) {
            if (!error) {
                boost::system::error_code ignored;
                storefrontAcceptor.close(ignored);
                adminAcceptor.close(ignored);
            }
        });
        acceptNext(false);
        acceptNext(true);
        context.run();
        handlers.join();
    }

    Tcp::acceptor& acceptor(bool adminListener) {
        return adminListener ? adminAcceptor : storefrontAcceptor;
    }

    void acceptNext(bool adminListener) {
        auto& listener = acceptor(adminListener);
        listener.async_accept(
            asio::make_strand(context),
            [this, adminListener](const boost::system::error_code& error,
                                  Tcp::socket socket) {
                if (!error) {
                    asio::post(handlers,
                               [this, adminListener,
                                client = std::move(socket)]() mutable {
                                   handle(std::move(client), adminListener);
                               });
                }
                if (acceptor(adminListener).is_open()) {
                    acceptNext(adminListener);
                }
            });
    }

    void handle(Tcp::socket socket, bool adminListener) {
        beast::flat_buffer buffer;
        http::request_parser<http::string_body> parser;
        parser.body_limit(128 * 1024);
        boost::system::error_code error;
        http::read(socket, buffer, parser, error);
        if (error) {
            return;
        }
        const auto request = parser.release();
        auto response = route(request, adminListener);
        response.version(request.version());
        response.keep_alive(false);
        response.prepare_payload();
        http::write(socket, response, error);
        socket.shutdown(Tcp::socket::shutdown_both, error);
    }

    http::response<http::string_body> response(http::status status,
                                                const std::string& contentType,
                                                std::string body,
                                                unsigned version = 11) const {
        http::response<http::string_body> output{status, version};
        output.set(http::field::server, "QueryFlow");
        output.set(http::field::content_type, contentType);
        output.body() = std::move(body);
        return output;
    }

    http::response<http::string_body> jsonResponse(http::status status,
                                                   const Json& body,
                                                   unsigned version) const {
        return response(status, "application/json; charset=utf-8", body.dump(), version);
    }

    Json metricsJson() const {
        const auto queries = manager.getQueries();
        const auto workers = manager.getWorkers();
        std::size_t queued = 0;
        std::size_t running = 0;
        std::size_t completed = 0;
        std::size_t failed = 0;
        std::vector<double> waitingTimes;
        std::vector<double> executionTimes;
        std::vector<double> responseTimes;
        auto firstSubmission = std::chrono::steady_clock::time_point::max();

        for (const auto& query : queries) {
            firstSubmission = std::min(firstSubmission, query.submitTime);
            switch (query.status) {
            case QueryStatus::Queued:
                ++queued;
                break;
            case QueryStatus::Running:
                ++running;
                break;
            case QueryStatus::Completed: {
                ++completed;
                const auto metrics = QueryMetrics::fromSnapshot(query);
                waitingTimes.push_back(metrics.waitingTimeMs());
                executionTimes.push_back(metrics.executionTimeMs());
                responseTimes.push_back(metrics.responseTimeMs());
                break;
            }
            case QueryStatus::Failed:
                ++failed;
                break;
            }
        }

        double throughput = 0.0;
        if (completed > 0) {
            const auto elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - firstSubmission)
                                     .count();
            if (elapsed > 0.0) {
                throughput = static_cast<double>(completed) / elapsed;
            }
        }
        const auto activeWorkers = static_cast<std::size_t>(std::count_if(
            workers.begin(), workers.end(), [](const auto& worker) {
                return worker.busy;
            }));

        Json output = {
            {"totalSubmitted", queries.size()},
            {"queued", queued},
            {"running", running},
            {"completed", completed},
            {"failed", failed},
            {"activeWorkers", activeWorkers},
            {"workerCount", workers.size()},
            {"queueLength", manager.queueSize()},
            {"cumulativeThroughputPerSecond", throughput},
            {"averageSampleSize", completed}
        };
        setOptional(output, "averageWaitingMs", average(waitingTimes));
        setOptional(output, "averageExecutionMs", average(executionTimes));
        setOptional(output, "averageResponseMs", average(responseTimes));
        return output;
    }

    http::response<http::string_body> route(
        const http::request<http::string_body>& request,
        bool adminListener) {
        const auto target = std::string(request.target());
        const auto queryStart = target.find('?');
        const auto path = target.substr(0, queryStart);

        const bool adminAsset = path == "/admin" || path == "/admin.js" ||
                                (adminListener && path == "/");
        const bool adminApi = path == "/api/config" || path == "/api/queries" ||
                              path.rfind("/api/queries/", 0) == 0 ||
                              path == "/api/queue" || path == "/api/workers" ||
                              path == "/api/events" || path == "/api/metrics";
        if ((adminListener && !adminAsset && !adminApi && path != "/styles.css") ||
            (!adminListener && (adminAsset || adminApi))) {
            return jsonResponse(http::status::not_found,
                                {{"error", "Not found"}},
                                request.version());
        }

        if (request.method() == http::verb::post &&
            (path == "/api/register" || path == "/api/login")) {
            try {
                const auto body = Json::parse(request.body());
                if (!body.is_object()) {
                    throw std::invalid_argument("Request body must be an object");
                }
                const auto account = path == "/api/register"
                                        ? registerCustomer(body)
                                        : signIn(body);
                return jsonResponse(
                    path == "/api/register" ? http::status::created : http::status::ok,
                    {{"token", account.second},
                     {"customer", accountJson(std::stoull(account.first))}},
                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::get && path == "/api/account") {
            try {
                return jsonResponse(http::status::ok,
                                    accountJson(customerFor(request)),
                                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::post && path == "/api/logout") {
            const auto header = request.find("X-QueryFlow-Session");
            if (header != request.end()) {
                std::lock_guard<std::mutex> lock(sessionsMutex);
                const auto value = header->value();
                sessions.erase(std::string(value.data(), value.size()));
            }
            return jsonResponse(http::status::ok, {{"ok", true}}, request.version());
        }
        if (request.method() == http::verb::get && path == "/api/products") {
            try {
                const auto query = executeQuery(
                    "SELECT p.product_id, p.sku, p.product_name, p.description, "
                    "c.category_name, CAST(p.unit_price AS CHAR) AS unit_price, "
                    "p.stock_quantity "
                    "FROM products p JOIN categories c ON c.category_id = p.category_id "
                    "WHERE p.active = TRUE ORDER BY c.category_name, p.product_name",
                    {}, 20, {}, "catalog-read");
                return jsonResponse(http::status::ok,
                                    rowObjects(query),
                                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::get && path == "/api/cart") {
            try {
                const auto customerId = customerFor(request);
                const auto query = executeQuery(
                    "SELECT p.product_id, p.product_name, CAST(p.unit_price AS CHAR) "
                    "AS unit_price, ci.quantity, p.stock_quantity "
                    "FROM shopping_carts c "
                    "JOIN shopping_cart_items ci ON ci.cart_id = c.cart_id "
                    "JOIN products p ON p.product_id = ci.product_id "
                    "WHERE c.customer_id = ? ORDER BY p.product_name",
                    {std::to_string(customerId)}, 20, std::to_string(customerId),
                    "cart-read");
                return jsonResponse(http::status::ok,
                                    rowObjects(query),
                                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::post && path == "/api/cart/items") {
            try {
                const auto customerId = customerFor(request);
                const auto body = Json::parse(request.body());
                if (!body.is_object() || !body.contains("productId") ||
                    !body["productId"].is_number_integer() ||
                    !body.contains("quantity") || !body["quantity"].is_number_integer()) {
                    throw std::invalid_argument(
                        "A positive productId and quantity are required");
                }
                const auto productId = body["productId"].get<std::int64_t>();
                const auto quantity = body["quantity"].get<std::int64_t>();
                if (productId < 1 || quantity < 1 || quantity > 99) {
                    throw std::invalid_argument("Quantity must be between 1 and 99");
                }
                const auto query = executeQuery(
                    "INSERT INTO shopping_cart_items (cart_id, product_id, quantity) "
                    "SELECT c.cart_id, ?, ? "
                    "FROM shopping_carts c "
                    "JOIN products p ON p.product_id = ? AND p.active = TRUE "
                    "WHERE c.customer_id = ? "
                    "ON DUPLICATE KEY UPDATE quantity = LEAST(quantity + VALUES(quantity), 999)",
                    {std::to_string(productId), std::to_string(quantity),
                     std::to_string(productId), std::to_string(customerId)},
                    50, std::to_string(customerId), "cart-add");
                if (query.affectedRowsAvailable && query.affectedRows == 0) {
                    throw ApiError(http::status::not_found,
                                   "Product is unavailable");
                }
                return jsonResponse(http::status::created,
                                    {{"ok", true}, {"engineQueryId", query.id}},
                                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::delete_ &&
            path.rfind("/api/cart/items/", 0) == 0) {
            try {
                const auto customerId = customerFor(request);
                const auto idText = path.substr(std::string("/api/cart/items/").size());
                std::uint64_t productId = 0;
                const auto parsed = std::from_chars(
                    idText.data(), idText.data() + idText.size(), productId);
                if (parsed.ec != std::errc{} ||
                    parsed.ptr != idText.data() + idText.size() || productId == 0) {
                    throw std::invalid_argument("Invalid product ID");
                }
                const auto query = executeQuery(
                    "DELETE ci FROM shopping_cart_items ci "
                    "JOIN shopping_carts c ON c.cart_id = ci.cart_id "
                    "WHERE c.customer_id = ? AND ci.product_id = ?",
                    {std::to_string(customerId), std::to_string(productId)},
                    40, std::to_string(customerId), "cart-remove");
                if (!query.affectedRowsAvailable || query.affectedRows == 0) {
                    throw ApiError(http::status::not_found,
                                   "Cart item was not found");
                }
                return jsonResponse(http::status::ok,
                                    {{"ok", true}, {"engineQueryId", query.id}},
                                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::post && path == "/api/checkout") {
            try {
                const auto customerId = customerFor(request);
                const auto body = Json::parse(request.body());
                if (!body.is_object()) {
                    throw std::invalid_argument("Request body must be an object");
                }
                const auto recipient = boundedString(body, "recipientName", "", 120);
                const auto address = boundedString(body, "addressLine", "", 240);
                const auto city = boundedString(body, "city", "", 100);
                const auto region = boundedString(body, "region", "", 100);
                const auto postalCode = boundedString(body, "postalCode", "", 24);
                const auto country = boundedString(body, "country", "India", 80);
                const auto checkout = executeQuery(
                    "CHECKOUT", {std::to_string(customerId), recipient, address, city,
                                  region, postalCode, country},
                    100, std::to_string(customerId), "checkout");
                if (checkout.rows.empty() || checkout.rows.front().size() < 7) {
                    throw std::runtime_error(
                        "Could not load the confirmed order receipt");
                }
                const auto orderId = checkout.rows.front()[0];
                Json items = Json::array();
                for (const auto& row : checkout.rows) {
                    if (row.size() < 7) {
                        throw std::runtime_error(
                            "Could not load the confirmed order receipt");
                    }
                    items.push_back({{"product_name_snapshot", row[4]},
                                     {"quantity", row[5]},
                                     {"unit_price", row[6]}});
                }
                const auto metrics = QueryMetrics::fromSnapshot(checkout);
                return jsonResponse(
                    http::status::created,
                    {{"orderId", orderId},
                     {"status", checkout.rows.front()[1]},
                     {"paymentStatus", checkout.rows.front()[2]},
                     {"total", checkout.rows.front()[3]},
                     {"shippingAddress", recipient + ", " + address + ", " + city +
                                             ", " + region + " " + postalCode +
                                             ", " + country},
                     {"items", std::move(items)},
                     {"confirmation",
                      {{"message", "Order confirmed. Payment is simulated; no money was charged."},
                       {"queryId", checkout.id},
                       {"workerId", checkout.workerId},
                       {"waitingMs", metrics.waitingTimeMs()},
                       {"executionMs", metrics.executionTimeMs()},
                       {"responseMs", metrics.responseTimeMs()},
                       {"status", toString(checkout.status)}}}},
                    request.version());
            } catch (const std::exception& exception) {
                const std::string message = exception.what();
                if (message == "Your cart is empty" ||
                    message == "An item in your cart is out of stock" ||
                    message == "An item in your cart is unavailable") {
                    return jsonResponse(http::status::conflict,
                                        {{"error", message}},
                                        request.version());
                }
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }
        if (request.method() == http::verb::get && path == "/api/orders") {
            try {
                const auto customerId = customerFor(request);
                const auto query = executeQuery(
                    "SELECT o.order_id, o.order_status, o.payment_status, "
                    "CAST(o.total_amount AS CHAR) AS total_amount, o.order_date, "
                    "o.shipping_address_snapshot, oi.product_name_snapshot, "
                    "oi.quantity, CAST(oi.unit_price AS CHAR) AS unit_price "
                    "FROM orders o LEFT JOIN order_items oi ON oi.order_id = o.order_id "
                    "WHERE o.customer_id = ? "
                    "ORDER BY o.order_date DESC, oi.order_item_id",
                    {std::to_string(customerId)}, 20, std::to_string(customerId),
                    "order-history");
                return jsonResponse(http::status::ok,
                                    rowObjects(query),
                                    request.version());
            } catch (const std::exception& exception) {
                return jsonResponse(errorStatus(exception),
                                    {{"error", exception.what()}},
                                    request.version());
            }
        }

        if (request.method() == http::verb::get && path == "/api/health") {
            return jsonResponse(http::status::ok, {{"ok", true}}, request.version());
        }
        if (request.method() == http::verb::get && path == "/api/config") {
            return jsonResponse(
                http::status::ok,
                {{"backend", backend},
                 {"workerCount", manager.workerCount()},
                 {"policy", policyName(manager.schedulingPolicy())},
                 {"runtimeReconfigurationSupported", false},
                 {"submissionMode", "prepared-shopping-operations"}},
                request.version());
        }
        if (request.method() == http::verb::get && path == "/api/queries") {
            Json output = Json::array();
            for (const auto& query : manager.getQueries()) {
                output.push_back(queryJson(query));
            }
            return jsonResponse(http::status::ok, output, request.version());
        }
        if (request.method() == http::verb::get && path == "/api/queue") {
            Json output = Json::array();
            std::size_t position = 1;
            for (const auto& query : manager.getPendingQueries()) {
                output.push_back({
                    {"position", position++},
                    {"id", query.id},
                    {"arrivalSequence", query.arrivalSequence},
                    {"sql", query.sql},
                    {"userId", query.userId},
                    {"sessionLabel", query.sessionLabel},
                    {"operation", query.operation},
                    {"priority", query.priority},
                    {"submittedAt", epochMilliseconds(query.submittedAt)},
                    {"waitingMs", std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - query.submitTime)
                                      .count()}
                });
            }
            return jsonResponse(http::status::ok, output, request.version());
        }
        if (request.method() == http::verb::get && path == "/api/workers") {
            Json output = Json::array();
            for (const auto& worker : manager.getWorkers()) {
                output.push_back(workerJson(worker));
            }
            return jsonResponse(http::status::ok, output, request.version());
        }
        if (request.method() == http::verb::get && path == "/api/events") {
            const auto after = parseAfter(target);
            Json output = Json::array();
            for (const auto& event : manager.getEvents()) {
                if (event.eventId > after) {
                    output.push_back(eventJson(event));
                }
            }
            return jsonResponse(http::status::ok, output, request.version());
        }
        if (request.method() == http::verb::get && path == "/api/metrics") {
            return jsonResponse(http::status::ok, metricsJson(), request.version());
        }
        if (request.method() == http::verb::get && path.rfind("/api/queries/", 0) == 0) {
            int id = 0;
            const auto idText = path.substr(std::string("/api/queries/").size());
            const auto parsed = std::from_chars(
                idText.data(), idText.data() + idText.size(), id);
            if (parsed.ec != std::errc{} || parsed.ptr != idText.data() + idText.size()) {
                return jsonResponse(http::status::bad_request,
                                    {{"error", "Invalid query ID"}},
                                    request.version());
            }
            const auto query = manager.getQuery(id);
            if (!query.has_value()) {
                return jsonResponse(http::status::not_found,
                                    {{"error", "Query not found"}},
                                    request.version());
            }
            Json output = queryJson(*query);
            output["events"] = Json::array();
            for (const auto& event : manager.getEvents()) {
                if (event.queryId == id) {
                    output["events"].push_back(eventJson(event));
                }
            }
            return jsonResponse(http::status::ok, output, request.version());
        }
        if (request.method() == http::verb::get && path == "/" && adminListener) {
            return staticResponse("admin.html", "text/html; charset=utf-8", request.version());
        }
        if (request.method() == http::verb::get && path == "/") {
            return staticResponse("index.html", "text/html; charset=utf-8", request.version());
        }
        if (request.method() == http::verb::get && path == "/admin") {
            return staticResponse("admin.html", "text/html; charset=utf-8", request.version());
        }
        if (request.method() == http::verb::get && path == "/app.js") {
            return staticResponse("app.js", "text/javascript; charset=utf-8", request.version());
        }
        if (request.method() == http::verb::get && path == "/admin.js") {
            return staticResponse("admin.js", "text/javascript; charset=utf-8", request.version());
        }
        if (request.method() == http::verb::get && path == "/styles.css") {
            return staticResponse("styles.css", "text/css; charset=utf-8", request.version());
        }
        return jsonResponse(http::status::not_found,
                            {{"error", "Not found"}},
                            request.version());
    }

    http::response<http::string_body> staticResponse(const std::string& filename,
                                                      const std::string& contentType,
                                                      unsigned version) const {
#ifndef QUERYFLOW_WEB_DIR
#define QUERYFLOW_WEB_DIR "web"
#endif
        const auto path = std::filesystem::path(QUERYFLOW_WEB_DIR) / filename;
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return jsonResponse(http::status::not_found,
                                {{"error", "Web asset not found"}},
                                version);
        }
        std::ostringstream content;
        content << file.rdbuf();
        auto body = content.str();
        if (filename == "admin.html") {
            const auto marker = body.find("{{STOREFRONT_URL}}");
            if (marker != std::string::npos) {
                body.replace(marker, std::string("{{STOREFRONT_URL}}").size(),
                             "http://127.0.0.1:" + std::to_string(storefrontPort) + "/");
            }
        }
        return response(http::status::ok, contentType, std::move(body), version);
    }
};

ApiServer::ApiServer(QueryManager& manager,
                     std::string backend,
                     std::string storefrontHost,
                     unsigned short storefrontPort,
                     unsigned short adminPort)
    : impl_(std::make_unique<Impl>(manager,
                                   std::move(backend),
                                   std::move(storefrontHost),
                                   storefrontPort,
                                   adminPort)) {}

ApiServer::~ApiServer() = default;

void ApiServer::run() {
    impl_->run();
}

}  // namespace queryflow