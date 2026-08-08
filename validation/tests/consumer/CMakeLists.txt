cmake_minimum_required(VERSION 3.24)
project(rasterm_consumer LANGUAGES CXX)

find_package(rasterm CONFIG REQUIRED)

add_executable(rasterm-consumer main.cpp)
target_link_libraries(rasterm-consumer PRIVATE rasterm::rasterm)
