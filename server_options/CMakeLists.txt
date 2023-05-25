cmake_minimum_required(VERSION 3.10)

project(server_options_example VERSION 1.0.0 LANGUAGES CXX)

find_package(Arrow REQUIRED)
find_package(ArrowFlight REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(CMAKE_BUILD_TYPE Debug)

add_executable(server server.cc)
set_property(TARGET server PROPERTY CXX_STANDARD 17)
# NOTE: We use arrow_shared here for older Arrow (like v9). Use
# Arrow::arrow_shared for newer releases
target_link_libraries(server PRIVATE arrow_shared arrow_flight_shared gRPC::grpc++)
