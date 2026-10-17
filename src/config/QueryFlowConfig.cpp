#include "queryflow/config/QueryFlowConfig.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace queryflow {
namespace {

std::string trimString(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n'\"");
    if (first == std::string::npos) return "";
    const auto last = str.find_last_not_of(" \t\r\n'\"");
    return str.substr(first, last - first + 1);
}

std::map<std::string, std::string> readEnvFile() {
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
            const auto key = trimString(line.substr(0, eqPos));
            const auto val = trimString(line.substr(eqPos + 1));
            if (!key.empty()) {
                env[key] = val;
            }
        }
        break;
    }
    return env;
}

std::string getEnvWithFallback(const char* key, const char* fallback) {
    const char* val = std::getenv(key);
    if (val != nullptr && *val != '\0') {
        return val;
    }
    static const auto dotEnv = readEnvFile();
    const auto it = dotEnv.find(key);
    if (it != dotEnv.end() && !it->second.empty()) {
        return it->second;
    }
    return fallback;
}

}  // namespace

QueryFlowConfig QueryFlowConfig::fromEnvironment() {
    QueryFlowConfig config;

    const auto workersText = getEnvWithFallback("QUERYFLOW_WORKERS", "4");
    try {
        const int parsedWorkers = std::stoi(workersText);
        if (parsedWorkers > 0) {
            config.maxWorkers = parsedWorkers;
        }
    } catch (...) {}

    const auto policyText = getEnvWithFallback("QUERYFLOW_POLICY", "fcfs");
    config.enablePriorityScheduling = (policyText == "priority");

    config.backend = getEnvWithFallback("QUERYFLOW_BACKEND", "demo");
    config.host = getEnvWithFallback("QUERYFLOW_DB_HOST", "127.0.0.1");

    const auto portText = getEnvWithFallback("QUERYFLOW_DB_PORT", "3306");
    try {
        const int parsedPort = std::stoi(portText);
        if (parsedPort > 0 && parsedPort <= 65535) {
            config.port = parsedPort;
        }
    } catch (...) {}

    config.databaseName = getEnvWithFallback("QUERYFLOW_DB_NAME", "queryflow");
    config.user = getEnvWithFallback("QUERYFLOW_DB_USER", "queryflow_app");
    config.password = getEnvWithFallback("QUERYFLOW_DB_PASSWORD", "");

    return config;
}

}  // namespace queryflow
