cmake_minimum_required(VERSION 3.16)

project(QueryFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(QueryFlow
    src/app/main.cpp
    src/scheduler/Scheduler.cpp
    src/queue/SharedQueryQueue.cpp
    src/worker/WorkerPool.cpp
    src/database/DatabaseManager.cpp
    src/database/MockDatabaseManager.cpp
    src/manager/QueryManager.cpp
    src/metrics/QueryMetrics.cpp
    src/config/QueryFlowConfig.cpp
)

target_include_directories(QueryFlow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

add_executable(SchedulerTests
    tests/scheduler_test.cpp
    src/scheduler/Scheduler.cpp
)

target_include_directories(SchedulerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME SchedulerTests COMMAND SchedulerTests)

add_executable(ConcurrencyTests
    tests/concurrency_test.cpp
    src/queue/SharedQueryQueue.cpp
    src/worker/WorkerPool.cpp
    src/database/MockDatabaseManager.cpp
)

target_include_directories(ConcurrencyTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME ConcurrencyTests COMMAND ConcurrencyTests)