#include "queryflow/security/PasswordHash.h"

#include <array>
#include <stdexcept>
#include <vector>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

namespace queryflow {
namespace {

constexpr int kPasswordIterations = 310000;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 64;

std::string toHex(const unsigned char* bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string output(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        output[index * 2] = digits[bytes[index] >> 4];
        output[index * 2 + 1] = digits[bytes[index] & 0x0f];
    }
    return output;
}

bool fromHex(const std::string& input, std::vector<unsigned char>& output) {
    if (input.empty() || input.size() % 2 != 0) {
        return false;
    }
    output.clear();
    output.reserve(input.size() / 2);
    for (std::size_t index = 0; index < input.size(); index += 2) {
        const auto digit = [](char value) -> int {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        };
        const auto high = digit(input[index]);
        const auto low = digit(input[index + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        output.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return true;
}

std::array<unsigned char, kHashBytes> derive(
    const std::string& password,
    const std::vector<unsigned char>& salt) {
    std::array<unsigned char, kHashBytes> output{};
    if (PKCS5_PBKDF2_HMAC(password.data(),
                          static_cast<int>(password.size()),
                          salt.data(),
                          static_cast<int>(salt.size()),
                          kPasswordIterations,
                          EVP_sha256(),
                          static_cast<int>(output.size()),
                          output.data()) != 1) {
        throw std::runtime_error("Password hashing failed");
    }
    return output;
}

}  // namespace

std::pair<std::string, std::string> hashPassword(const std::string& password) {
    std::array<unsigned char, kSaltBytes> salt{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("Secure random generator failed");
    }
    std::vector<unsigned char> saltBytes(salt.begin(), salt.end());
    const auto hash = derive(password, saltBytes);
    return {toHex(hash.data(), hash.size()), toHex(salt.data(), salt.size())};
}

bool verifyPassword(const std::string& password,
                    const std::string& saltHex,
                    const std::string& expectedHashHex) {
    std::vector<unsigned char> salt;
    std::vector<unsigned char> expected;
    if (!fromHex(saltHex, salt) || salt.size() != kSaltBytes ||
        !fromHex(expectedHashHex, expected) || expected.size() != kHashBytes) {
        return false;
    }
    const auto actual = derive(password, salt);
    return CRYPTO_memcmp(actual.data(), expected.data(), actual.size()) == 0;
}

std::string createSessionToken() {
    std::array<unsigned char, 32> token{};
    if (RAND_bytes(token.data(), static_cast<int>(token.size())) != 1) {
        throw std::runtime_error("Secure random generator failed");
    }
    return toHex(token.data(), token.size());
}

}  // namespace queryflow
