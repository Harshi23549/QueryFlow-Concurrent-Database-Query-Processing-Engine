# QueryFlow Team Module Guide

This project is structured so every team member can work on a specific part without depending on the whole system being mixed into a single folder.

## 1. Module map

- `include/queryflow/core`  
  Shared data models such as `Query`.

- `include/queryflow/queue`  
  Thread-safe producer-consumer queue.

- `include/queryflow/scheduler`  
  FCFS and priority-based scheduling logic.

- `include/queryflow/worker`  
  Worker thread pool that consumes queued tasks.

- `include/queryflow/database`  
  Database abstraction layer and mock/real database implementations.

- `include/queryflow/manager`  
  Query submission and orchestration layer.

- `include/queryflow/metrics`  
  Each query's waiting, execution, and response timings.

- `include/queryflow/config`  
  Runtime configuration and future tuning parameters.

- `include/queryflow/batch` and `src/batch`
  SQL workload file parsing, submission through QueryManager, and batch report
  generation.

- `src/app`  
  CLI application entry point and interactive commands.

- `src/*`  
  Concrete implementation files for each module.

- `tests`  
  Validation and concurrency checks.

## 2. Team responsibilities

- Core Engine & Integration: architecture, module wiring, build health
- Concurrency & Worker System: queue and thread pool behavior
- Scheduling & Performance: scheduler policy and metrics
- Database & SQL: MySQL connectivity and query execution integration

## 3. Module responsibilities

### Core
Contains the core query object used across the pipeline.

### Queue
Handles safe communication between producers and workers.

### Scheduler
Chooses which query should run next under FCFS or priority rules.

### Worker
Executes queued queries using the database abstraction.

### Database
Keeps database execution pluggable. The demo backend is used by tests; the
merged MySQL Connector/C++ backend can be enabled with
`QUERYFLOW_ENABLE_MYSQL=ON`.

### Manager
Assigns query IDs, validates submissions, tracks lifecycle state, coordinates
the queue and worker pool, and exposes snapshots to the CLI.

### Metrics
Tracks the performance numbers needed for analysis and reports.

### Config
Provides a place for tuning worker count, policy selection, database settings, and deployment options.

### CLI
The current CLI is in `src/app/main.cpp`. It accepts SQL text or an explicit
`--file` workload, reports status/timing, and can select either demo or (when
built with the connector) MySQL using `--backend`.

## 4. How to contribute

1. Pick a module.
2. Add or update only the files inside that module.
3. Keep public interfaces in `include/queryflow/...`.
4. Put implementations in `src/...`.
5. Add tests under `tests` for behavior changes.

## 5. Build commands

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
