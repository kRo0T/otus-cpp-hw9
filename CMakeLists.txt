cmake_minimum_required(VERSION 3.9)
project(async)

set(CMAKE_CXX_STANDARD 14)

add_library(async STATIC async.cpp async.h)

add_executable(async_cli main.cpp)

target_link_libraries(async_cli async)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)