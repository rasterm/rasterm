cmake_minimum_required(VERSION 3.24)
project(rasterm_c_consumer LANGUAGES C)

find_package(rasterm CONFIG REQUIRED)

add_executable(rasterm-c-consumer main.c)
target_link_libraries(rasterm-c-consumer PRIVATE rasterm::rasterm)
