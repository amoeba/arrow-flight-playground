cmake_minimum_required(VERSION 3.19)
project(arrow-flight-server)

set(CMAKE_CXX_STANDARD 17)

find_package(Arrow CONFIG)

add_executable(
    server
    server.cc
)

target_link_libraries(server
    parquet
    arrow_flight
    arrow
)
