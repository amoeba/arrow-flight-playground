cmake_minimum_required(VERSION 3.19)
project(arrow-distributed-flight-server)

set(CMAKE_CXX_STANDARD 17)

find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(Arrow CONFIG REQUIRED)

add_executable(
    coordinator
    coordinator.cc
)

target_link_libraries(coordinator
    parquet
    arrow_flight
    arrow
    opentelemetry-cpp::otlp_grpc_exporter
    opentelemetry-cpp::ostream_span_exporter
)

add_executable(
    server
    server.cc
)

target_link_libraries(server
    parquet
    arrow_flight
    arrow
    opentelemetry-cpp::otlp_grpc_exporter
    opentelemetry-cpp::ostream_span_exporter
)
