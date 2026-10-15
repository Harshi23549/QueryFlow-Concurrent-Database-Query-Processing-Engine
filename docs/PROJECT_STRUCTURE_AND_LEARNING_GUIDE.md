# QueryFlow Project Structure and Learning Guide

This document records the restructuring work done on QueryFlow, explains the
reasoning behind it, and provides a repeatable approach for organizing and
debugging similar C++ projects.

It is also intended as a handoff document for every team member. The repository
is the shared source of truth: after this document and the code are committed
and pushed, teammates can pull the branch and see the same structure.

## 1. What we wanted

QueryFlow is an Operating Systems and Database Systems project. Its goal is to
accept SQL requests, organize and schedule them, run them using multiple worker
threads, communicate with a database, and measure performance.

The initial request was to make the project less messy and divide it into
modules. The follow-up clarified that the structure should be visible and
understandable to every team member, not merely moved into folders. The work
therefore had two goals:

1. Put related headers, implementations, the application entry point, and tests
   into predictable places.
2. Document what each module owns, how modules relate, how the team can
   contribute, and how to build and test the project.

Organizing files is not the same as completing every planned feature. The
current tree contains working queue, worker-pool, and scheduler components,
alongside scaffolding for future integration. See [Implementation status](#8-implementation-status)
before describing the system as fully integrated.

## 2. What changed

Before restructuring, several C++ source and header files were at the
repository root. This made it harder to see which files belonged together,
where new code should go, and what should be built for each test.

The code was reorganized into:

```text
include/queryflow/       Public module headers
src/                     Implementations, grouped by module
src/app/                 Application entry point
tests/                   Scheduler and concurrency tests
docs/                    Team and project documentation
```

The old root-level copies of the moved source files were removed to avoid two
competing versions of the same code. `CMakeLists.txt` was updated to compile
the new paths. `Makefile` was also updated to use the new source locations.
The main `README.md` and `docs/TEAM_MODULE_GUIDE.md` describe the project layout
and team responsibilities.

## 3. Why this structure

### Public headers and implementation files

`include/queryflow/...` contains declarations that other modules need to use.
`src/...` contains the code that implements those declarations.

For example:

```text
include/queryflow/queue/SharedQueryQueue.h
src/queue/SharedQueryQueue.cpp
```

The header describes how callers can use the queue. The `.cpp` file contains
the synchronization and queue operations. This separation gives modules a
clear interface and keeps implementation details out of unrelated code.

### Module-oriented folders

Related files live together. A developer working on scheduling can start in
`include/queryflow/scheduler` and `src/scheduler`; a developer working on
database execution can start in `include/queryflow/database` and
`src/database`.

The goal is not to create folders for their own sake. A module should group
one responsibility behind a clear interface. If a module is still planned,
its files are a scaffold until meaningful behavior and tests are added.

### Tests have their own directory

Tests are not application code. Keeping them under `tests/` makes it easier to
find them and lets CMake build and run them separately from the application.

### Build files describe the project

`CMakeLists.txt` is the build manifest: it states which source files make each
executable and which tests CTest should run. When source files move, the build
manifest must move with them; otherwise the tree may look organized but fail
to compile.

## 4. Current directory map

```text
QueryFlow/
├── CMakeLists.txt
├── Makefile
├── README.md
├── include/queryflow/
│   ├── config/       QueryFlowConfig.h
│   ├── core/         Query.h
│   ├── database/     IDatabaseManager.h, DatabaseManager.h,
│   │                 MockDatabaseManager.h
│   ├── manager/      QueryManager.h
│   ├── metrics/      QueryMetrics.h
│   ├── queue/        SharedQueryQueue.h
│   ├── scheduler/    Scheduler.h
│   └── worker/       WorkerPool.h
├── src/
│   ├── app/          main.cpp
│   ├── config/       QueryFlowConfig.cpp
│   ├── database/     DatabaseManager.cpp, MockDatabaseManager.cpp
│   ├── manager/      QueryManager.cpp
│   ├── metrics/      QueryMetrics.cpp
│   ├── queue/        SharedQueryQueue.cpp
│   ├── scheduler/    Scheduler.cpp
│   └── worker/       WorkerPool.cpp
├── tests/
│   ├── concurrency_test.cpp
│   ├── engine_test.cpp
│   └── scheduler_test.cpp
└── docs/
    ├── PROJECT_STRUCTURE_AND_LEARNING_GUIDE.md
    └── TEAM_MODULE_GUIDE.md
```

`build/` is generated output created by CMake. It is not source code and
should not be used as a place to edit project files.

## 5. How the intended system fits together

The intended request path is:

```text
Client / CLI
    -> Query Manager
    -> Shared Queue and Scheduler
    -> Worker Pool
    -> Database Manager
    -> Result and Metrics
```

The modules have these responsibilities:

| Module | Responsibility | Main location |
|---|---|---|
| Core | Shared data types, such as the query submitted for execution | `include/queryflow/core` |
| Queue | Thread-safe handoff of queued work to consumers | `include/queryflow/queue`, `src/queue` |
| Scheduler | Chooses a task according to FCFS or priority rules | `include/queryflow/scheduler`, `src/scheduler` |
| Worker | Owns reusable worker threads and invokes database execution | `include/queryflow/worker`, `src/worker` |
| Database | Defines the execution interface and provides mock/real implementations | `include/queryflow/database`, `src/database` |
| Manager | Coordinates query submission and the lifecycle of requests | `include/queryflow/manager`, `src/manager` |
| Metrics | Measures waiting, execution, and response times | `include/queryflow/metrics`, `src/metrics` |
| Config | Holds runtime choices such as worker count and database settings | `include/queryflow/config`, `src/config` |
| App | Starts and wires the application | `src/app` |
| Tests | Checks behavior independently of the production entry point | `tests` |

An interface such as `IDatabaseManager` is useful because the worker pool can
depend on a stable contract instead of a specific database library. A mock
implementation can be used in concurrency tests; a real MySQL implementation
can later fulfill the same contract.

## 6. Important terms

- **Module:** A group of code with one clear responsibility and an interface
  used by other parts of the program.
- **Header (`.h`):** A C++ file that declares types and functions for use by
  other files. It commonly contains class declarations and public interfaces.
- **Implementation (`.cpp`):** A C++ file that defines the functions declared
  in a header.
- **Include path:** A directory the compiler searches when resolving
  `#include` directives. The project adds `include/`, which is why code can
  write `#include "queryflow/queue/SharedQueryQueue.h"`.
- **CMake:** The build-system generator used here to describe targets and
  tests. It generates build files for the local compiler/toolchain.
- **Target:** A named program or library produced by a build system. `QueryFlow`,
  `SchedulerTests`, and `ConcurrencyTests` are executable targets.
- **CTest:** CMake's test runner. It runs tests registered with `add_test`.
- **Thread pool:** A fixed group of reusable threads that take work from a
  shared source rather than creating one new thread for every query.
- **Producer-consumer queue:** A synchronization pattern where producers add
  work and consumers wait for and remove it.
- **Mutex:** A mutual-exclusion lock. It protects shared state so only one
  thread at a time can modify or inspect that state in a protected section.
- **Condition variable:** A thread coordination tool. A worker can sleep while
  the queue is empty and be notified when work or a stop request arrives.
- **FCFS (First-Come, First-Served):** Select the task with the earliest
  submission time.
- **Priority scheduling:** Select the task with the highest configured
  priority. The current scheduler breaks equal-priority ties by earlier
  submission time.
- **Mock:** A substitute implementation used for tests. The mock database
  manager simulates latency and records executed query IDs; it does not run SQL.
- **Scaffold:** A placeholder that establishes a location or interface for
  future work but is not a completed feature.
- **Integration:** Connecting modules so data actually flows through their
  interfaces as one working system. Merely compiling the modules in one
  executable does not prove they are integrated.

## 7. Debugging and validation performed

The debugging approach was to verify observable behavior, not infer it from
filenames:

1. **Inspect the repository.** The source showed separate queue, worker-pool,
   scheduler, mock database, and test code at the root.
2. **Build and run the existing configured tests.** CMake built successfully,
   and the initially configured scheduler test passed. At that point, the
   CMake test list did not yet include the concurrency stress test.
3. **Run the concurrency test directly.** The first manual compile failed at
   link time with undefined references to `WorkerPool` methods. The command
   compiled the test and queue/mock sources but omitted `WorkerPool.cpp`.
4. **Use the linker error to find the missing input.** Adding
   `WorkerPool.cpp` to the compile command resolved those undefined symbols.
   The test then passed for worker counts 1, 2, and 4 and for a multi-producer
   workload.
5. **Wire that test into CMake.** The concurrency test became a normal CMake
   target and CTest entry so future contributors do not have to remember a
   separate manual compile command.
6. **Move the project into modules.** Headers, implementations, and tests were
   moved/recreated under their module paths; include statements and build
   source lists were changed accordingly.
7. **Build and test after restructuring.** CMake regenerated, all targets
   compiled, and CTest reported both registered tests passing.
8. **Document the team layout.** The README and team guide were updated so
   teammates can identify ownership and find the right files.

The final verification command was:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The observed result at restructuring time was 2/2 tests passing. This validates
the scheduler test and the mock-backed queue/worker concurrency test; it does
not validate MySQL execution or a complete end-to-end submission workflow.

### General debugging method to reuse

When a C++ change fails:

1. Read the **first** compiler or linker error, not only the final summary.
2. Classify it:
   - compiler error: a source/header/type/include problem;
   - linker error (`undefined reference`): a function was declared or used but
     its implementation was not linked, or its signature does not match;
   - test failure: the program built but behavior did not match an assertion;
   - runtime failure: inspect the exact failing operation and its inputs.
3. Check the build target's source list and include directories.
4. Make one focused fix.
5. Rebuild the smallest affected target, then run its test.
6. Run the full relevant test suite before handing the change to the team.
7. Record what was tested and what was not tested; a passing mock test is not
   proof of a real database integration.

## 8. Implementation status

This distinction prevents the folder structure from overstating the project's
actual capabilities:

### Implemented and tested

- **Shared queue:** Uses a mutex and condition variable for thread-safe
  producer/consumer access, rejects pushes after shutdown, and selects the next
  pending item using the configured FCFS or priority policy.
- **Worker pool:** Starts worker threads that pop queries and call the
  `IDatabaseManager` interface; execution exceptions are returned to the
  manager as failed query status.
- **Mock database manager:** Simulates execution time and records IDs for the
  concurrency tests. The CLI makes clear that SQL is not executed by this
  backend.
- **Scheduler:** Implements FCFS and priority selection, including earlier
  submission as the equal-priority tie-breaker. The queue uses this scheduler
  to choose among pending queries.
- **Query manager:** Assigns IDs, validates empty input, records queued/running/
  completed/failed status, returns query snapshots, and waits for outstanding
  work.
- **Metrics:** Waiting, execution, and response times are collected from query
  lifecycle timestamps and displayed by the CLI.
- **CLI:** Supports SQL-line submission, explicit priority, status inspection,
  waiting for completion, and graceful draining on exit.
- **Tests/build structure:** CMake builds the CLI and registers scheduler,
  concurrency, and engine lifecycle tests.

### Optional / environment-dependent

- **MySQL backend:** The merged Connector/C++ adapter is wired into the CLI
  behind `QUERYFLOW_ENABLE_MYSQL`. The default build remains demo-only so it
  can compile without connector libraries. MySQL mode still requires the
  Connector/C++ development package, a running server, credentials, and the
  supplied schema. This environment did not have the connector installed, so
  the MySQL-enabled compilation and live server execution still need testing.
- **Configuration:** The config struct is not yet loaded from a file or CLI
  options. Worker count and scheduling policy are currently CLI options.

## 9. How to add a feature in this project

Use this sequence for future work:

1. **Define the behavior.** Write down inputs, outputs, failure cases, and
   concurrency expectations.
2. **Choose the owning module.** Put the public contract in
   `include/queryflow/<module>/` and implementation in `src/<module>/`.
3. **Reuse the existing interfaces.** For example, database implementations
   should satisfy `IDatabaseManager` rather than changing the worker pool to
   depend on a concrete connector.
4. **Add or update tests.** Put tests in `tests/`; test normal behavior,
   boundary conditions, errors, and concurrent interactions where relevant.
5. **Update CMake.** Add newly compiled implementation/test files to the
   correct target.
6. **Build and run focused tests.** Then run the full CTest suite.
7. **Update documentation.** Describe only behavior that is implemented and
   verified; label future work as planned or scaffolded.
8. **Share the changes.** Commit and push them to the team branch/repository
   so every member can pull the same code and documentation.

## 10. Build and test commands

From the repository root:

```bash
# Generate or refresh the build directory
cmake -S . -B build

# Compile all configured targets
cmake --build build

# Run all registered tests and print failures
ctest --test-dir build --output-on-failure
```

The Makefile also provides convenience targets:

```bash
make
make run
make test
```

If the build directory has stale generated files after a significant build
configuration change, use a fresh build directory (for example `build-clean`)
instead of deleting source files or editing generated files.

## 11. How every team member sees the same project

Changes in one developer's local working directory are not automatically
visible to teammates. To share this work, it must be committed to the intended
Git branch and pushed to the shared GitHub repository. Teammates then fetch or
pull that branch. Coordinate before merging if multiple members are changing
the same module.

The documentation describes the intended ownership areas, but folder
organization does not enforce exclusive ownership. Team members can review
each other's changes, and changes to shared interfaces should be coordinated
because they affect dependent modules.
