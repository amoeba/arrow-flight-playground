cmake_minimum_required(VERSION 3.19)
project(arrow-cookbook)

set(CMAKE_CXX_STANDARD 17)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
endif()

# Add Arrow and other required packages
find_package(Arrow REQUIRED)
if(NOT ${ARROW_VERSION} VERSION_GREATER "9.0.0")
  get_filename_component(ARROW_CMAKE_BASE_DIR ${Arrow_CONFIG} DIRECTORY)
  list(INSERT CMAKE_MODULE_PATH 0 ${ARROW_CMAKE_BASE_DIR})
endif()
find_package(ArrowFlight REQUIRED)
find_package(Parquet REQUIRED)

add_executable(
    server
    server.cc
)

target_link_libraries(server parquet_shared arrow_flight_shared)
target_link_libraries(parquet_shared INTERFACE arrow_shared)
target_link_libraries(arrow_flight_shared INTERFACE arrow_shared)
