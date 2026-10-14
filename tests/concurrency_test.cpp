#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "queryflow/database/MockDatabaseManager.h"
#include "queryflow/queue/SharedQueryQueue.h"
#include "queryflow/worker/WorkerPool.h"

namespace {

bool verifyExactlyOnce(std::vector<int> ids, size_t expectedCount) {
    if (ids.size() != expectedCount) {
        std::cerr << "  FAIL: expected " << expectedCount << " executions, got "
                  << ids.size() << "\n";
        return false;
    }
    std::sort(ids.begin(), ids.end());
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] != static_cast<int>(i)) {
            std::cerr << "  FAIL: id " << i << " missing or duplicated (found "
                      << ids[i] << " at sorted position " << i << ")\n";
            return false;
        }
    }
    return true;
}

bool runSingleProducerTest(size_t numWorkers, int queryCount) {
    std::cout << "[single-producer] workers=" << numWorkers
              << " queries=" << queryCount << " ... ";

    queryflow::SharedQueryQueue queue;
    queryflow::MockDatabaseManager db(std::chrono::milliseconds(1));
    queryflow::WorkerPool pool(numWorkers, queue, db);

    pool.start();
    for (int i = 0; i < queryCount; ++i) {
        queue.push(queryflow::Query(i, "SELECT " + std::to_string(i)));
    }
    pool.shutdown();

    bool ok = verifyExactlyOnce(db.executedIds(), static_cast<size_t>(queryCount));
    std::cout << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

bool runMultiProducerBurstTest(size_t numWorkers, size_t numProducers,
                              int queriesPerProducer) {
    const int totalQueries = static_cast<int>(numProducers) * queriesPerProducer;
    std::cout << "[multi-producer]  workers=" << numWorkers
              << " producers=" << numProducers
              << " queries/producer=" << queriesPerProducer
              << " (total=" << totalQueries << ") ... ";

    queryflow::SharedQueryQueue queue;
    queryflow::MockDatabaseManager db(std::chrono::milliseconds(1));
    queryflow::WorkerPool pool(numWorkers, queue, db);

    pool.start();

    std::vector<std::thread> producers;
    producers.reserve(numProducers);
    for (size_t p = 0; p < numProducers; ++p) {
        producers.emplace_back([&queue, p, queriesPerProducer] {
            int base = static_cast<int>(p) * queriesPerProducer;
            for (int i = 0; i < queriesPerProducer; ++i) {
                queue.push(queryflow::Query(base + i, "SELECT " + std::to_string(base + i)));
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }

    pool.shutdown();

    bool ok = verifyExactlyOnce(db.executedIds(), static_cast<size_t>(totalQueries));
    std::cout << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
}

}  // namespace

int main() {
    bool allPassed = true;

    std::cout << "=== QueryFlow concurrency test suite ===\n\n";

    for (size_t workers : {1u, 2u, 4u}) {
        allPassed &= runSingleProducerTest(workers, 500);
    }

    std::cout << "\n";
    allPassed &= runMultiProducerBurstTest(/*numWorkers=*/4, /*numProducers=*/5,
                                          /*queriesPerProducer=*/200);

    std::cout << "\n" << (allPassed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";
    return allPassed ? EXIT_SUCCESS : EXIT_FAILURE;
}
