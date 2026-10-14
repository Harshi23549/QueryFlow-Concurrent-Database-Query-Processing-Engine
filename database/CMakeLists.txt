
cmake_minimum_required(VERSION 3.16)
project(queryflow_database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MYSQL_CONCPP_DIR "C:/Program Files/MySQL/MySQL Connector C++ 26.7")

add_library(queryflow_database Database.cpp)

target_include_directories(queryflow_database PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MYSQL_CONCPP_DIR}/include"
)

target_link_directories(queryflow_database PUBLIC
    "${MYSQL_CONCPP_DIR}/lib64/vs14"
)

target_link_libraries(queryflow_database PUBLIC
    mysqlcppconn
)

add_executable(queryflow_db_demo db_demo.cpp)
target_link_libraries(queryflow_db_demo PRIVATE queryflow_database)
