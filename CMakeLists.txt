cmake_minimum_required(VERSION 3.22)
project(ProtocolStack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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


add_executable(ProtocolStack
        src/main.cpp
)

target_include_directories(ProtocolStack PRIVATE
        ${DPDK_INCLUDE_DIRS}
        ${GTEST_INCLUDE_DIRS}
)

target_link_directories(ProtocolStack PRIVATE ${DPDK_LIBRARY_DIRS})

target_link_libraries(ProtocolStack PRIVATE
        ${DPDK_LIBRARIES}
        GTest::gtest GTest::gtest_main
        PRIVATE spdlog::spdlog_header_only
)

target_compile_options(ProtocolStack PRIVATE -O3 -Wall -g -msse4.1)