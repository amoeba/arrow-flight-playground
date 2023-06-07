cmake_minimum_required(VERSION 3.19)
project(arrow-distributed-flight-server)

set(CMAKE_CXX_STANDARD 17)

find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(Arrow CONFIG REQUIRED)

add_executable(
    metadata_server
    metadata_server.cc
)

target_link_libraries(metadata_server
    parquet
    arrow_flight
    arrow
    opentelemetry-cpp::otlp_grpc_exporter
    opentelemetry-cpp::ostream_span_exporter
)

add_executable(
    data_server
    data_server.cc
)

target_link_libraries(data_server
    parquet
    arrow_flight
    arrow
    opentelemetry-cpp::otlp_grpc_exporter
    opentelemetry-cpp::ostream_span_exporter
)
