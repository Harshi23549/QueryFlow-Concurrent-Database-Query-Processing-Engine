#pragma once

#include <string>
#include <utility>

namespace queryflow {

std::pair<std::string, std::string> hashPassword(const std::string& password);
bool verifyPassword(const std::string& password,
                    const std::string& saltHex,
                    const std::string& expectedHashHex);
std::string createSessionToken();

}  // namespace queryflow
