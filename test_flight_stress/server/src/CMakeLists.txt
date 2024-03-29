cmake_minimum_required(VERSION 3.19)
project(arrow-flight-server)

set(CMAKE_CXX_STANDARD 17)

# include(ExternalProject)

# ExternalProject_Add(
#   absl_ep
#   GIT_REPOSITORY https://github.com/abseil/abseil-cpp
#   GIT_TAG        20230125.1
#   CMAKE_ARGS
#     -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#     -DCMAKE_VERBOSE_BUILD:BOOL=ON
#     -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
#     -DCMAKE_CXX_FLAGS=-fPIC
# )

# ExternalProject_Add(
#   gRPC_ep
#   DEPENDS absl_ep
#   GIT_REPOSITORY https://github.com/grpc/grpc
#   GIT_TAG        v1.35.0
#   CMAKE_ARGS
#     -DgRPC_INSTALL=ON
#     -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#     -DCMAKE_VERBOSE_BUILD:BOOL=ON
#     -DgRPC_ABSL_PROVIDER=package
#     -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
#     -DCMAKE_CXX_FLAGS=-fPIC
# )

# ExternalProject_Add_StepDependencies(gRPC_ep build absl_ep)

# ExternalProject_Add(
#   arrow_ep
# #   DEPENDS gRPC_ep
# #   GIT_REPOSITORY "https://github.com/apache/arrow.git"
# #   GIT_TAG        main
#   URL https://dlcdn.apache.org/arrow/arrow-11.0.0/apache-arrow-11.0.0.tar.gz
#   URL_HASH SHA256=2dd8f0ea0848a58785628ee3a57675548d509e17213a2f5d72b0d900b43f5430
# #   URL https://dlcdn.apache.org/arrow/arrow-9.0.0/apache-arrow-9.0.0.tar.gz
# #   URL_HASH SHA256=a9a033f0a3490289998f458680d19579cf07911717ba65afde6cb80070f7a9b5
# #   URL https://dlcdn.apache.org/arrow/arrow-8.0.0/apache-arrow-8.0.0.tar.gz
# #   URL_HASH SHA256=ad9a05705117c989c116bae9ac70492fe015050e1b80fb0e38fde4b5d863aaa3
#   GIT_SHALLOW    TRUE
#   SOURCE_SUBDIR cpp
#   CMAKE_ARGS
#   -DARROW_WITH_SNAPPY=ON
#   -DARROW_COMPUTE=ON
#   -DARROW_PARQUET=ON
#   -DARROW_FLIGHT=ON
#   -DARROW_FILESYSTEM=ON
#   -DARROW_WITH_OPENTELEMETRY=ON
#   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#   -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
#   -DCMAKE_VERBOSE_BUILD:BOOL=ON
#   # By default, we will use the dependencies provided by Arrow build
#   -DARROW_DEPENDENCY_SOURCE=BUNDLED
#   # But we will use gRPC and its dependencies from the gRPC external project.
#   -Dabsl_SOURCE=SYSTEM
#   -DRE2_SOURCE=SYSTEM
#   -DProtobuf_SOURCE=SYSTEM
#   -DgRPC_SOURCE=SYSTEM
#   -Dopentelemetry-cpp_SOURCE=SYSTEM
# )

# ExternalProject_Add_StepDependencies(arrow_ep build gRPC_ep)

find_package(Arrow CONFIG)
# find_package(opentelemetry-cpp CONFIG)
# find_package(Protobuf CONFIG)
# find_package(gRPC)

add_executable(
    server
    server.cc
)
# add_dependencies(server arrow_ep)

target_link_libraries(server
    parquet
    arrow_flight
    arrow
    opentelemetry-cpp::otlp_grpc_exporter
    opentelemetry-cpp::ostream_span_exporter
)
