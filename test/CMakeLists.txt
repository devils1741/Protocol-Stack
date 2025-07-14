cmake_minimum_required(VERSION 3.22)
project(UT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# project header
include_directories(../include)

# GoogleTest
find_package(GTest REQUIRED)

# splog
if(NOT TARGET spdlog)
        # Stand-alone build
        find_package(spdlog REQUIRED)
endif()


# DPDK
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)


include_directories(arp.hpp)
add_executable(UtArp
        UtArp.cpp
)

target_include_directories(UtArp PRIVATE
        ${DPDK_INCLUDE_DIRS}
        ${GTEST_INCLUDE_DIRS}
)

target_link_directories(UtArp PRIVATE ${DPDK_LIBRARY_DIRS})

target_link_libraries(UtArp PRIVATE
        ${DPDK_LIBRARIES}
        GTest::gtest GTest::gtest_main
        PRIVATE spdlog::spdlog_header_only
        pthread
)

target_compile_options(UtArp PRIVATE -O3 -Wall -g -msse4.1)
