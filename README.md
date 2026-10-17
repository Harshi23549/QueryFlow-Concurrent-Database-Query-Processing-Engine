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
│       ├── batch/
│       │   ├── BatchReport.h
│       │   ├── BatchRunner.h
│       │   └── SqlFileReader.h
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
│   ├── batch/
│   │   ├── BatchReport.cpp
│   │   ├── BatchRunner.cpp
│   │   └── SqlFileReader.cpp
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
│   ├── batch_test.cpp
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
- `include/queryflow/batch` and `src/batch` – SQL file parser, batch execution,
  CSV and human-readable reports
- `tests/batch_test.cpp` – parser, mock execution, report and failure-path tests
- `tests/scheduler_test.cpp` – scheduler validation
- `tests/concurrency_test.cpp` – concurrency stress verification

## Technologies

- **C++17**
- **CMake**
- **MySQL Connector/C++** (optional runtime backend)
- **Linux**
- **Git & GitHub**

## Current verified deliverables

The following are implemented and validated:

- thread-safe queue
- worker thread pool
- FCFS and priority scheduling integrated with pending queue selection
- query lifecycle tracking and waiting/execution/response metrics
- interactive CLI for query submission and status/results
- concurrency stress tests
- modular CMake build
- demo backend plus an optional MySQL Connector/C++ backend (live DB requires
  local connector/server configuration)
- automated database initialization and seeding scripts (`scripts/setup_mysql.sh`, `scripts/start_mysql.sh`, `scripts/stop_mysql.sh`)
- `.env` configuration file support for database credentials and connection parameters
- SQL file batch execution with concurrent workers, per-query status/metrics,
  and CSV/log reports

## Quick start

### 1. Default demo build (no database required)

```bash
make
make test
./build/QueryFlow --workers 4 --policy fcfs
```

### 2. Live MySQL build

To set up the database and run QueryFlow with real MySQL query execution:

```bash
# 1. Initialize, start MySQL server instance, and load schema/seed data
make db-setup

# 2. Build with MySQL Connector/C++ enabled
make build-mysql

# 3. Run all unit and integration tests
make test-mysql

# 4. Start QueryFlow CLI with live MySQL backend
make run-mysql
```

Alternatively, run directly with flags:
```bash
./build-mysql/QueryFlow --backend mysql --workers 4 --policy priority
```

### SQL workload batch mode

An explicitly supplied SQL file runs in batch mode through the same
QueryManager, scheduler, queue, worker pool, and selected database backend.
For example:

```bash
./build-mysql/QueryFlow \
  --backend mysql \
  --file database/workload.sql \
  --workers 4 \
  --policy FCFS \
  --priority 0 \
  --log-dir logs
```

The demo backend can also exercise the batch workflow without a database:

```bash
./build/QueryFlow --backend demo --file database/workload.sql \
  --workers 4 --policy priority --log-dir logs
```

The demo backend simulates execution and does not parse/execute SQL. The MySQL
backend executes the statements and requires successful database setup.
Reports are written to unique `logs/queryflow-batch-<timestamp>.log` and
`.csv` paths. Quoted and numeric SQL literal values are redacted in report SQL
fields; do not put credentials in SQL text.

The file reader supports ordinary statements separated by semicolons,
multiline statements, `--` and `#` line comments, `/* ... */` block comments,
quoted strings/identifiers, and a final statement without a semicolon. It is a
statement splitter, not a complete MySQL script parser: stored routines,
`DELIMITER` directives, and other delimiter-changing scripts are unsupported.
Malformed quotes/comments and files with no statements are rejected before
execution. A database syntax/runtime error is reported against that
statement’s number, while remaining accepted statements continue.

Batch elapsed time begins immediately before the first submission and ends
when all accepted queries reach a terminal state. Throughput is completed
queries divided by that elapsed interval. Average wait/execution/response
metrics include completed queries only; failed or missing timings are not
treated as zero. Returned-row counts are reported only for result-set queries,
and affected-row counts only when provided by the backend. Unavailable values
are written as `N/A`.

You can also run the standalone database adapter demo:
```bash
make db-demo
```

### Database management commands

- `make db-setup` – initialize isolated database directory, start server, apply schema and seed data, and generate `.env`
- `make db-start` – start the MySQL server daemon
- `make db-stop`  – stop the MySQL server daemon
- `make db-demo`  – compile and execute the transactional C++ database demo

Connection configuration is loaded automatically from `.env` in the project root, or can be specified via environment variables or CLI flags:

```bash
./build-mysql/QueryFlow --backend mysql \
    --db-host 127.0.0.1 \
    --db-port 3307 \
    --db-name queryflow \
    --db-user queryflow_app \
    --db-password queryflow_pass
```

The MySQL backend gives each worker thread its own connector connection.
Submitted statements are executed concurrently across worker threads and return column headers, result rows, affected row counts, and latency metrics.


CLI commands:

- Type a SQL statement to submit it at priority `0`.
- `/submit <priority> <SQL>` submits a query with an explicit priority.
- `/status` lists query IDs and states.
- `/wait` waits until all submitted work finishes and prints results and
  timing metrics.
- `/quit` drains queued work and exits.

Use `./build/QueryFlow --help` for usage. The selected backend is printed at
startup.

## Module ownership

This structure allows each team member to work on a specific module without mixing all logic into one file:

- Core, Manager, App: Harshita
- Queue, Worker System: Disha
- Scheduler, Metrics: Sakshi
- Database, SQL Integration: Tanisha

For more details, see [docs/TEAM_MODULE_GUIDE.md](./docs/TEAM_MODULE_GUIDE.md).
