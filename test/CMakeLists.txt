CMAKE_MINIMUM_REQUIRED(VERSION 3.8)
PROJECT(test_logger)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3 -Wall -fPIC")

include_directories(../src)
aux_source_directory(../src LOG_DIR)

add_executable(test test.cc ${LOG_DIR})
target_link_libraries(test rt pthread)