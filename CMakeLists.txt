cmake_minimum_required(VERSION 3.16)

project(QueryFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(QueryFlow
    main.cpp
    scheduler.cpp
)

target_include_directories(QueryFlow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

enable_testing()

add_executable(SchedulerTests
    scheduler_test.cpp
    scheduler.cpp
)

target_include_directories(SchedulerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME SchedulerTests COMMAND SchedulerTests)

add_executable(ConcurrencyTests
    ConcurrencyTest.cpp
    SharedQueryQueue.cpp
    WorkerPool.cpp
    MockDatabaseManager.cpp
)

target_include_directories(ConcurrencyTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ConcurrencyTests COMMAND ConcurrencyTests)