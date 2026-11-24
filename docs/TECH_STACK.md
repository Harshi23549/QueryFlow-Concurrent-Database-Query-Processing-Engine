# Technology stack

| Technology | Purpose and location | If removed/replaced |
|---|---|---|
| C++17 | Engine, API, CLI, tests: `src/`, `include/`, `tests/` | Core application would need a rewrite. |
| CMake 3.16+ | Build definitions: `CMakeLists.txt`, `database/CMakeLists.txt` | Another build system must express targets and optional MySQL linkage. |
| GNU Make | Convenience commands: `Makefile` | CMake commands still work directly. |
| C++ threads, mutexes, atomics, condition variables | Queue, pool, snapshots: `SharedQueryQueue`, `WorkerPool`, `QueryManager` | Concurrent producer/consumer execution would not be safe. |
| Boost.Asio/Beast (Boost 1.74+) | Loopback HTTP server: `src/web/ApiServer.cpp` | API server must be replaced. |
| nlohmann-json 3.2+ | JSON parsing/serialization in API | HTTP data format code must be replaced. |
| OpenSSL Crypto | PBKDF2 password hashing and secure random tokens: `src/security/PasswordHash.cpp` | Account password hashing and session token generation cannot build. |
| HTML, CSS, vanilla JavaScript | Browser dashboard: `web/index.html`, `web/styles.css`, `web/app.js` | There is no React/Node framework to replace; a new client would be needed. |
| Fetch API + browser storage | Storefront requests and session token: `web/app.js` | Frontend would lose API communication/session persistence. |
| MySQL Connector/C++ | Optional SQL execution: `database/Database.cpp`, `DatabaseManager.cpp` | Live MySQL backend cannot build. |
| MySQL/InnoDB and SQL scripts | Schema/seed/workloads: `database/*.sql`; scripts | The mock backend remains usable, but no persistent shopping data. |
| Bash | Local MySQL setup/start/stop scripts | Users must configure MySQL manually. |

The source uses no Node.js, Express, React, WebSocket, Server-Sent Events, ORM, or separate API service. `Threads::Threads`, Boost, and nlohmann-json are direct CMake dependencies. MySQL is enabled only with `-DQUERYFLOW_ENABLE_MYSQL=ON`.

### Configuration

`QueryFlowConfig` and the database adapter load environment variables (and the first found `.env` among current, parent, grandparent): `QUERYFLOW_DB_HOST`, `QUERYFLOW_DB_PORT`, `QUERYFLOW_DB_NAME`, `QUERYFLOW_DB_USER`, and `QUERYFLOW_DB_PASSWORD`. CLI database flags set those variables for the process.
