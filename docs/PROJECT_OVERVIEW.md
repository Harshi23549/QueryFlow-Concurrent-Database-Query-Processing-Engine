# QueryFlow project overview

QueryFlow is an application-level query scheduling and execution layer with a
local shopping storefront. It is not a database or replacement for MySQL.
Customers perform normal shopping actions; the API translates them into
prepared SQL statements and submits them to the shared query engine. A
separate operations page shows live engine state.

## Project value

- OS concepts: worker threads, mutexes, condition variables, and
  producer/consumer coordination.
- DBMS concepts: MySQL schema design, foreign keys, prepared statements,
  transaction boundaries, inventory updates, and order history.
- Query processing: FCFS and priority scheduling, worker limits, lifecycle
  tracking, and performance metrics.
- Application flow: user actions mapped to database operations through an
  HTTP/JSON API.

## Practical example

The storefront does not expose SQL to the customer. Adding an item to a cart
submits a parameterized database operation; placing an order invokes a
transaction that checks stock, saves the order and its line items, records a
simulated payment and confirmation history, and clears the cart. QueryFlow's
manager and scheduler track these operations while worker threads execute
them against MySQL.

## Implementation status

| Area | Status |
|---|---|
| Thread-safe pending queue and workers | Implemented |
| Priority + FCFS tie-breaking and pure FCFS | Implemented |
| Lifecycle snapshots, worker state, and events | Implemented in process memory |
| Mock backend and CLI/batch modes | Implemented |
| Optional MySQL execution | Implemented behind a CMake option |
| Accounts, catalog, persistent cart, checkout and history | Implemented for local use |
| Password hashing and token sessions | Implemented; sessions are in memory |
| Storefront and engine operations page | Implemented |
| Persistent engine telemetry | Not implemented |
| Operations-page authentication and roles | Not implemented |
| Public deployment hardening | Not implemented |

## Important limitations

The web server binds to loopback. The operations page has no authentication.
Payments are simulated and no payment processor is connected. Query snapshots,
worker state, and lifecycle events are not persisted across process restarts.
Worker count and scheduling policy are selected at startup. The `mock`
backend is for engine development and does not provide persistent shopping
data.
