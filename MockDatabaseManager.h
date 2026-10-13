#pragma once

#include <chrono>
#include <mutex>
#include <vector>

#include "IDatabaseManager.h"
#include "Query.h"

// Stand-in for the real Database Manager so this module can run and be
// demoed on its own, before Tanisha's MySQL-backed implementation of
// IDatabaseManager is wired in. Delete this file (and its .cpp) once the
// real one is in place -- WorkerPool doesn't know or care which one it's
// talking to.
//
// It simulates DB latency with a short sleep and records every executed
// query id (thread-safely) so tests can verify each query was executed
// exactly once, regardless of how many workers were running.
class MockDatabaseManager : public IDatabaseManager {
public:
    // simulatedLatency: how long execute() pretends the query takes.
    explicit MockDatabaseManager(std::chrono::milliseconds simulatedLatency =
                                      std::chrono::milliseconds(2));

    void execute(const Query& query) override;

    // Test/inspection helper: snapshot of every query id that has been
    // executed so far, in the order execute() completed for each. Safe to
    // call while workers are still running.
    std::vector<int> executedIds() const;

private:
    std::chrono::milliseconds simulatedLatency_;
    mutable std::mutex mutex_;
    std::vector<int> executedIds_;
};
