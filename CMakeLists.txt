cmake_minimum_required(VERSION 3.16)

project(QueryFlow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
option(QUERYFLOW_ENABLE_MYSQL
    "Build with the merged MySQL Connector/C++ database module" OFF)
enable_testing()

add_library(queryflow_core
    src/batch/BatchReport.cpp
    src/batch/BatchRunner.cpp
    src/batch/SqlFileReader.cpp
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

if(QUERYFLOW_ENABLE_MYSQL)
    find_path(MYSQL_CONCPP_INCLUDE_DIR
        NAMES mysql/jdbc.h mysql_driver.h
        HINTS
            ${MYSQL_CONCPP_ROOT}
            $ENV{MYSQL_CONCPP_ROOT}
            ${CMAKE_CURRENT_SOURCE_DIR}/deps/mysql-connector
            /tmp/queryflow-mysql-connector/usr
        PATH_SUFFIXES include
    )
    find_library(MYSQL_CONCPP_LIBRARY
        NAMES mysqlcppconn
        HINTS
            ${MYSQL_CONCPP_ROOT}
            $ENV{MYSQL_CONCPP_ROOT}
            ${CMAKE_CURRENT_SOURCE_DIR}/deps/mysql-connector
            /tmp/queryflow-mysql-connector/usr
        PATH_SUFFIXES lib lib64 lib64/vs14 lib/x86_64-linux-gnu
    )
    find_library(MYSQL_CLIENT_LIBRARY
        NAMES mysqlclient mysqlclient.so.21
        HINTS
            ${MYSQL_CONCPP_ROOT}
            $ENV{MYSQL_CONCPP_ROOT}
            ${CMAKE_CURRENT_SOURCE_DIR}/deps/mysql-client
            /tmp/queryflow-mysql-client/usr
        PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
    )
    find_path(BOOST_INCLUDE_DIR
        NAMES boost/shared_ptr.hpp
        HINTS
            ${BOOST_ROOT}
            $ENV{BOOST_ROOT}
            ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost
            /tmp/queryflow-boost/usr
            /tmp/queryflow-boost/usr/include
        PATH_SUFFIXES include
    )

    if(NOT MYSQL_CONCPP_INCLUDE_DIR OR NOT MYSQL_CONCPP_LIBRARY)
        message(FATAL_ERROR
            "MySQL Connector/C++ was not found. Set MYSQL_CONCPP_ROOT, "
            "MYSQL_CONCPP_INCLUDE_DIR and MYSQL_CONCPP_LIBRARY, or configure "
            "with -DQUERYFLOW_ENABLE_MYSQL=OFF to build the demo backend.")
    endif()

    target_sources(queryflow_core PRIVATE database/Database.cpp)
    target_include_directories(queryflow_core PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/database
        ${MYSQL_CONCPP_INCLUDE_DIR}
    )
    if(BOOST_INCLUDE_DIR)
        target_include_directories(queryflow_core PRIVATE ${BOOST_INCLUDE_DIR})
    endif()

    target_link_libraries(queryflow_core PUBLIC ${MYSQL_CONCPP_LIBRARY})
    if(MYSQL_CLIENT_LIBRARY)
        target_link_libraries(queryflow_core PUBLIC ${MYSQL_CLIENT_LIBRARY})
    endif()

    get_filename_component(MYSQL_CONCPP_LIBDIR "${MYSQL_CONCPP_LIBRARY}" DIRECTORY)
    set(QF_RPATH_DIRS "${MYSQL_CONCPP_LIBDIR}")
    if(MYSQL_CLIENT_LIBRARY)
        get_filename_component(MYSQL_CLIENT_LIBDIR "${MYSQL_CLIENT_LIBRARY}" DIRECTORY)
        list(APPEND QF_RPATH_DIRS "${MYSQL_CLIENT_LIBDIR}")
    endif()

    set(CMAKE_BUILD_RPATH ${QF_RPATH_DIRS})
    set(CMAKE_INSTALL_RPATH ${QF_RPATH_DIRS})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")

    target_compile_definitions(queryflow_core PUBLIC QUERYFLOW_WITH_MYSQL=1)
endif()


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

add_executable(BatchTests tests/batch_test.cpp)
target_link_libraries(BatchTests PRIVATE queryflow_core)
target_compile_options(BatchTests PRIVATE -UNDEBUG)
add_test(NAME BatchTests COMMAND BatchTests)
