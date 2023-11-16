cmake_minimum_required(VERSION 3.26)
project(mini-redis)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/util.h)

add_executable(server server.cpp)
add_executable(client client.cpp)
