#include "queryflow/batch/SqlFileReader.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace queryflow {
namespace {

enum class State {
    Normal,
    SingleQuoted,
    DoubleQuoted,
    BacktickQuoted,
    LineComment,
    BlockComment
};

bool isDashComment(const std::string& input, std::size_t position) {
    if (position + 1 >= input.size() || input[position] != '-' ||
        input[position + 1] != '-') {
        return false;
    }
    return position + 2 >= input.size() ||
           static_cast<unsigned char>(input[position + 2]) <= ' ';
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

}  // namespace

std::vector<SqlStatement> readSqlStatements(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open SQL workload file: " + path.string());
    }

    const std::string input{
        std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        throw std::runtime_error("Error reading SQL workload file: " + path.string());
    }

    std::vector<SqlStatement> statements;
    std::string current;
    State state = State::Normal;
    std::size_t line = 1;
    std::size_t quoteStartLine = 0;
    std::size_t blockStartLine = 0;

    const auto finishStatement = [&] {
        std::string sql = trim(current);
        if (!sql.empty()) {
            statements.push_back({statements.size() + 1, std::move(sql)});
        }
        current.clear();
    };

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        const char next = i + 1 < input.size() ? input[i + 1] : '\0';

        if (state == State::LineComment) {
            if (ch == '\n') {
                current.push_back('\n');
                ++line;
                state = State::Normal;
            }
            continue;
        }
        if (state == State::BlockComment) {
            if (ch == '*' && next == '/') {
                current.push_back(' ');
                ++i;
                state = State::Normal;
            } else if (ch == '\n') {
                current.push_back('\n');
                ++line;
            }
            continue;
        }

        if (state == State::Normal) {
            if (isDashComment(input, i) || ch == '#') {
                current.push_back(' ');
                state = State::LineComment;
                if (ch == '-' && next == '-') {
                    ++i;
                }
                continue;
            }
            if (ch == '/' && next == '*') {
                current.push_back(' ');
                ++i;
                blockStartLine = line;
                state = State::BlockComment;
                continue;
            }
            if (ch == ';') {
                finishStatement();
                continue;
            }
            if (ch == '\'') {
                quoteStartLine = line;
                state = State::SingleQuoted;
            } else if (ch == '"') {
                quoteStartLine = line;
                state = State::DoubleQuoted;
            } else if (ch == '`') {
                quoteStartLine = line;
                state = State::BacktickQuoted;
            }
            current.push_back(ch);
            if (ch == '\n') {
                ++line;
            }
            continue;
        }

        const char quote = state == State::SingleQuoted
                               ? '\''
                               : state == State::DoubleQuoted ? '"' : '`';
        current.push_back(ch);
        if (ch == '\\' && state != State::BacktickQuoted && i + 1 < input.size()) {
            current.push_back(input[++i]);
            if (input[i] == '\n') {
                ++line;
            }
            continue;
        }
        if (ch == quote) {
            if (next == quote) {
                current.push_back(input[++i]);
                continue;
            }
            state = State::Normal;
        }
        if (ch == '\n') {
            ++line;
        }
    }

    if (state == State::BlockComment) {
        throw std::runtime_error(
            "Unterminated block comment in statement #" +
            std::to_string(statements.size() + 1) + ", starting at line " +
            std::to_string(blockStartLine));
    }
    if (state == State::SingleQuoted || state == State::DoubleQuoted ||
        state == State::BacktickQuoted) {
        throw std::runtime_error(
            "Unterminated quoted value or identifier in statement #" +
            std::to_string(statements.size() + 1) + ", starting at line " +
            std::to_string(quoteStartLine));
    }
    finishStatement();

    if (statements.empty()) {
        throw std::runtime_error("SQL workload file contains no statements");
    }
    return statements;
}

}  // namespace queryflow
