
cmake_minimum_required(VERSION 3.16)
project(queryflow_database LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(MYSQL_CONCPP_INCLUDE_DIR
    NAMES mysql/jdbc.h mysql_driver.h
    HINTS
        ${MYSQL_CONCPP_ROOT}
        $ENV{MYSQL_CONCPP_ROOT}
        /tmp/queryflow-mysql-connector/usr
        ${CMAKE_CURRENT_SOURCE_DIR}/../deps/mysql-connector
    PATH_SUFFIXES include
)
find_library(MYSQL_CONCPP_LIBRARY
    NAMES mysqlcppconn
    HINTS
        ${MYSQL_CONCPP_ROOT}
        $ENV{MYSQL_CONCPP_ROOT}
        /tmp/queryflow-mysql-connector/usr
        ${CMAKE_CURRENT_SOURCE_DIR}/../deps/mysql-connector
    PATH_SUFFIXES lib lib64 lib64/vs14 lib/x86_64-linux-gnu
)
find_library(MYSQL_CLIENT_LIBRARY
    NAMES mysqlclient
    HINTS
        ${MYSQL_CONCPP_ROOT}
        $ENV{MYSQL_CONCPP_ROOT}
        /tmp/queryflow-mysql-client/usr
        ${CMAKE_CURRENT_SOURCE_DIR}/../deps/mysql-client
    PATH_SUFFIXES lib lib64 lib/x86_64-linux-gnu
)
find_path(BOOST_INCLUDE_DIR
    NAMES boost/shared_ptr.hpp
    HINTS
        ${BOOST_ROOT}
        $ENV{BOOST_ROOT}
        /tmp/queryflow-boost/usr
        /tmp/queryflow-boost/usr/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../deps/boost
    PATH_SUFFIXES include
)

if(NOT MYSQL_CONCPP_INCLUDE_DIR OR NOT MYSQL_CONCPP_LIBRARY)
    message(FATAL_ERROR
        "MySQL Connector/C++ was not found. Set MYSQL_CONCPP_ROOT or "
        "MYSQL_CONCPP_INCLUDE_DIR and MYSQL_CONCPP_LIBRARY.")
endif()

add_library(queryflow_database Database.cpp)

target_include_directories(queryflow_database PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MYSQL_CONCPP_INCLUDE_DIR}"
)
if(BOOST_INCLUDE_DIR)
    target_include_directories(queryflow_database PUBLIC "${BOOST_INCLUDE_DIR}")
endif()

target_link_libraries(queryflow_database PUBLIC "${MYSQL_CONCPP_LIBRARY}")
if(MYSQL_CLIENT_LIBRARY)
    target_link_libraries(queryflow_database PUBLIC "${MYSQL_CLIENT_LIBRARY}")
endif()

add_executable(queryflow_db_demo db_demo.cpp)
target_link_libraries(queryflow_db_demo PRIVATE queryflow_database)
if(MYSQL_CLIENT_LIBRARY)
    target_link_libraries(queryflow_db_demo PRIVATE "${MYSQL_CLIENT_LIBRARY}")
endif()


set(QF_RPATH_DIRS "${MYSQL_CONCPP_LIBDIR}")
if(MYSQL_CLIENT_LIBRARY)
    get_filename_component(MYSQL_CLIENT_LIBDIR "${MYSQL_CLIENT_LIBRARY}" DIRECTORY)
    list(APPEND QF_RPATH_DIRS "${MYSQL_CLIENT_LIBDIR}")
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-new-dtags")

set_target_properties(queryflow_db_demo PROPERTIES
    BUILD_RPATH "${QF_RPATH_DIRS}"
    INSTALL_RPATH "${QF_RPATH_DIRS}"
)



