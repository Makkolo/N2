cmake_minimum_required(VERSION 3.10...4.2)
project(N2Gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(N2Lib STATIC n2Interpreter.cpp n2Interpreter.hpp tcpComs.cpp tcpComs.hpp)
target_include_directories(N2Lib PUBLIC include)

add_executable(N2Gateway source.cpp)
target_link_libraries(N2Gateway PUBLIC N2Lib)