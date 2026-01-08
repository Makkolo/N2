cmake_minimum_required(VERSION 3.10)
project(N2Gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wall -Wextra")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=armv8-a -mtune=cortex-a53")
endif()

add_executable(N2Gateway
    source.cpp
    n2Interpreter.cpp
    tcpComs.cpp
    )

target_link_libraries(N2Gateway pthread)

find_package(Threads REQUIRED)
target_link_libraries(N2Gateway Threads::Threads)