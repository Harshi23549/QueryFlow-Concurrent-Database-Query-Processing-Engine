#pragma once

#include <memory>
#include <string>

namespace queryflow {

class QueryManager;

class ApiServer {
public:
    ApiServer(QueryManager& manager,
              std::string backend,
              std::string storefrontHost,
              unsigned short storefrontPort,
              unsigned short adminPort);
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    ~ApiServer();

    void run();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace queryflow