
cmake_minimum_required(VERSION 3.16)
project(queryflow_database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(MYSQL_CONCPP_INCLUDE_DIR
    NAMES mysql/jdbc.h
    HINTS ${MYSQL_CONCPP_ROOT} $ENV{MYSQL_CONCPP_ROOT}
    PATH_SUFFIXES include
)
find_library(MYSQL_CONCPP_LIBRARY
    NAMES mysqlcppconn
    HINTS ${MYSQL_CONCPP_ROOT} $ENV{MYSQL_CONCPP_ROOT}
    PATH_SUFFIXES lib lib64 lib64/vs14
)

add_library(queryflow_database Database.cpp)

if(NOT MYSQL_CONCPP_INCLUDE_DIR OR NOT MYSQL_CONCPP_LIBRARY)
    message(FATAL_ERROR
        "MySQL Connector/C++ was not found. Set MYSQL_CONCPP_ROOT or "
        "MYSQL_CONCPP_INCLUDE_DIR and MYSQL_CONCPP_LIBRARY.")
endif()

target_include_directories(queryflow_database PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MYSQL_CONCPP_INCLUDE_DIR}"
)
target_link_libraries(queryflow_database PUBLIC "${MYSQL_CONCPP_LIBRARY}")

add_executable(queryflow_db_demo db_demo.cpp)
target_link_libraries(queryflow_db_demo PRIVATE queryflow_database)
