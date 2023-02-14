cmake_minimum_required(VERSION 3.10)

project(basic_doput_example VERSION 1.0.0 LANGUAGES CXX)

###

find_package(Arrow REQUIRED)
find_package(ArrowFlight REQUIRED)
find_package(Parquet REQUIRED)

###

add_executable(client client.cc)
set_property(TARGET client PROPERTY CXX_STANDARD 17)
# NOTE: We use arrow_shared here for older Arrow (like v9). Use
# Arrow::arrow_shared for newer releases
target_link_libraries(client PRIVATE arrow_shared arrow_flight_shared parquet_shared)

add_executable(server server.cc)
set_property(TARGET server PROPERTY CXX_STANDARD 17)
# NOTE: We use arrow_shared here for older Arrow (like v9). Use
# Arrow::arrow_shared for newer releases
target_link_libraries(server PRIVATE arrow_shared arrow_flight_shared parquet_shared)
