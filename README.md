# QueryFlow: Concurrent Database Query Processing Engine

**QueryFlow** is an OS + DBMS project that explores how multiple database queries can be processed concurrently in a structured and measurable way.

The project combines **multithreading, synchronization, worker threads, and scheduling** with **SQL execution, database integration, transactions, and performance analysis**.

## Team

- **Harshita Aggarwal** - Core Engine & Integration
- **Disha Suthar** - Concurrency & Worker System
- **Sakshi Tiwari** - Scheduling & Performance
- **Tanisha Pokhriyal** - Database & SQL

## Project goal

QueryFlow is designed to help the team understand how application-level scheduling interacts with database execution. The system receives SQL tasks, organizes them, schedules them, executes them concurrently, and records performance metrics for analysis.

## Project structure for the whole team

The repository is now organized by module so every team member can clearly see what belongs to which component.

```text
QueryFlow/
├── CMakeLists.txt
├── Makefile
├── README.md
├── include/
│   └── queryflow/
│       ├── config/
│       │   └── QueryFlowConfig.h
│       ├── core/
│       │   └── Query.h
│       ├── database/
│       │   ├── DatabaseManager.h
│       │   ├── IDatabaseManager.h
│       │   └── MockDatabaseManager.h
│       ├── manager/
│       │   └── QueryManager.h
│       ├── metrics/
│       │   └── QueryMetrics.h
│       ├── queue/
│       │   └── SharedQueryQueue.h
│       ├── scheduler/
│       │   └── Scheduler.h
│       └── worker/
│           └── WorkerPool.h
├── src/
│   ├── app/
│   │   └── main.cpp
│   ├── config/
│   │   └── QueryFlowConfig.cpp
│   ├── database/
│   │   ├── DatabaseManager.cpp
│   │   └── MockDatabaseManager.cpp
│   ├── manager/
│   │   └── QueryManager.cpp
│   ├── metrics/
│   │   └── QueryMetrics.cpp
│   ├── queue/
│   │   └── SharedQueryQueue.cpp
│   ├── scheduler/
│   │   └── Scheduler.cpp
│   ├── worker/
│   │   └── WorkerPool.cpp
│   └── ...
├── tests/
│   ├── scheduler_test.cpp
│   └── concurrency_test.cpp
├── docs/
│   └── TEAM_MODULE_GUIDE.md
└── build/
```

## Module breakdown

### 1. Core
Responsible for the shared query representation used through every stage of the system.

- `include/queryflow/core/Query.h`
- `Query` stores the SQL string, id, and submission time.

### 2. Queue
Responsible for producer-consumer synchronization.

- `include/queryflow/queue/SharedQueryQueue.h`
- `src/queue/SharedQueryQueue.cpp`

Features:
- thread-safe queue
- mutex + condition variable
- blocking pop until work is available
- clean shutdown behavior

### 3. Scheduler
Responsible for deciding which queued query runs next.

- `include/queryflow/scheduler/Scheduler.h`
- `src/scheduler/Scheduler.cpp`

Supported policies:
- FCFS
- Priority scheduling

Also calculates:
- waiting time
- execution time
- response time

### 4. Worker Pool
Responsible for executing queued tasks concurrently.

- `include/queryflow/worker/WorkerPool.h`
- `src/worker/WorkerPool.cpp`

Features:
- configurable worker count
- reusable threads
- database execution through interface
- safe shutdown

### 5. Database Layer
Responsible for the abstraction over actual database execution.

- `include/queryflow/database/IDatabaseManager.h`
- `include/queryflow/database/DatabaseManager.h`
- `include/queryflow/database/MockDatabaseManager.h`

This layer keeps the engine testable before real MySQL integration is added.

### 6. Query Manager
Responsible for managing query submission and overall orchestration.

- `include/queryflow/manager/QueryManager.h`
- `src/manager/QueryManager.cpp`

This is the future coordination layer between the client and the worker pool.

### 7. Metrics
Responsible for measuring and reporting execution performance.

- `include/queryflow/metrics/QueryMetrics.h`
- `src/metrics/QueryMetrics.cpp`

### 8. Config
Responsible for project configuration and tuning values.

- `include/queryflow/config/QueryFlowConfig.h`
- `src/config/QueryFlowConfig.cpp`

### 9. App and tests

- `src/app/main.cpp` – application startup point
- `tests/scheduler_test.cpp` – scheduler validation
- `tests/concurrency_test.cpp` – concurrency stress verification

## Technologies

- **C++17**
- **CMake**
- **MySQL** (planned integration)
- **Linux**
- **Git & GitHub**

## Current verified deliverables

The following are implemented and validated:

- thread-safe queue
- worker thread pool
- scheduler logic (FCFS + priority)
- metrics calculations
- concurrency stress tests
- modular CMake build

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Module ownership

This structure allows each team member to work on a specific module without mixing all logic into one file:

- Core, Manager, App: Harshita
- Queue, Worker System: Disha
- Scheduler, Metrics: Sakshi
- Database, SQL Integration: Tanisha

For more details, see [docs/TEAM_MODULE_GUIDE.md](./docs/TEAM_MODULE_GUIDE.md).
