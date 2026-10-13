#pragma once

#include "Query.h"

// Integration seam with the Database + MySQL module. Whoever implements the
// real database manager just needs to derive from this and implement
// execute(). WorkerPool only ever talks to this interface, so the mock and
// the real implementation are interchangeable.
class IDatabaseManager {
public:
    virtual ~IDatabaseManager() = default;

    // Run a single query against the database. Called from a worker thread
    // -- implementations must be safe to call concurrently from multiple
    // threads at once, since every worker shares the same IDatabaseManager
    // instance.
    virtual void execute(const Query& query) = 0;
};
