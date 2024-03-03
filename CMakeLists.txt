cmake_minimum_required(VERSION 3.22)
project(mini-redis)

set(CMAKE_CXX_STANDARD 17)

set(CXX_FLAGS "-std=gnu++17 -ggdb3 -O2 -Werror -Wall -Wextra -Wshadow -fPIC -fno-strict-aliasing -fwrapv -pthread -U_FORTIFY_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_HOME_DIRECTORY}/bin)


add_executable(mini-redis main.cpp reactor.cpp server.cpp worker.cpp util.cpp db.cpp req_parser.cpp robj.cpp robj_str.cpp)

