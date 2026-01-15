cmake_minimum_required(VERSION 3.10.0...4.2)
project(N2Gateway VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wall -Wextra")

add_library(N2Lib STATIC tools.cpp tools.hpp n2Interpreter.cpp n2Interpreter.hpp tcpComs.cpp tcpComs.hpp)
target_include_directories(N2Lib PUBLIC include)

add_executable(N2Gateway source.cpp)
target_link_libraries(N2Gateway PUBLIC N2Lib)