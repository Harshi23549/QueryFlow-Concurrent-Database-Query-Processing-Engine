cmake_minimum_required(VERSION 3.16)

project(QueryFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
enable_testing()

add_library(queryflow_core
    src/config/QueryFlowConfig.cpp
    src/database/DatabaseManager.cpp
    src/database/MockDatabaseManager.cpp
    src/manager/QueryManager.cpp
    src/metrics/QueryMetrics.cpp
    src/queue/SharedQueryQueue.cpp
    src/scheduler/Scheduler.cpp
    src/worker/WorkerPool.cpp
)

target_include_directories(queryflow_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(queryflow_core PUBLIC Threads::Threads)
target_compile_options(queryflow_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(QueryFlow src/app/main.cpp)
target_link_libraries(QueryFlow PRIVATE queryflow_core)
target_compile_options(QueryFlow PRIVATE -Wall -Wextra -Wpedantic)

add_executable(SchedulerTests tests/scheduler_test.cpp)
target_link_libraries(SchedulerTests PRIVATE queryflow_core)
target_compile_options(SchedulerTests PRIVATE -UNDEBUG)
add_test(NAME SchedulerTests COMMAND SchedulerTests)

add_executable(ConcurrencyTests tests/concurrency_test.cpp)
target_link_libraries(ConcurrencyTests PRIVATE queryflow_core)
target_compile_options(ConcurrencyTests PRIVATE -UNDEBUG)
add_test(NAME ConcurrencyTests COMMAND ConcurrencyTests)

add_executable(EngineTests tests/engine_test.cpp)
target_link_libraries(EngineTests PRIVATE queryflow_core)
target_compile_options(EngineTests PRIVATE -UNDEBUG)
add_test(NAME EngineTests COMMAND EngineTests)
