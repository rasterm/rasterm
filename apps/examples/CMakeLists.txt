# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.24)
project(rasterm-examples LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The rasterm examples currently require Windows Terminal.")
endif()

set(RASTERM_EXAMPLES_RASTERM_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH
    "rasterm source directory used when rasterm::rasterm is not already available")

if(NOT TARGET rasterm::rasterm)
    if(EXISTS "${RASTERM_EXAMPLES_RASTERM_SOURCE}/CMakeLists.txt")
        set(RASTERM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(RASTERM_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        set(RASTERM_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(RASTERM_BUILD_SHARED_C_API OFF CACHE BOOL "" FORCE)
        add_subdirectory("${RASTERM_EXAMPLES_RASTERM_SOURCE}"
            "${CMAKE_CURRENT_BINARY_DIR}/rasterm" EXCLUDE_FROM_ALL)
    else()
        find_package(rasterm 1.2 CONFIG REQUIRED)
    endif()
endif()

add_executable(rasterm-example-c c_api.c)
add_executable(rasterm-example-synchronous synchronous.cpp)
add_executable(rasterm-example-presenter presenter.cpp)
add_executable(rasterm-example-indexed indexed.cpp)
add_executable(rasterm-example-custom-sink custom_sink.cpp)
add_executable(rasterm-example-damage damage_rectangles.cpp)
add_executable(rasterm-example-color color_metadata.cpp)

foreach(example IN ITEMS
        rasterm-example-c rasterm-example-synchronous rasterm-example-presenter
        rasterm-example-indexed rasterm-example-custom-sink rasterm-example-damage
        rasterm-example-color)
    target_link_libraries(${example} PRIVATE rasterm::rasterm)
endforeach()
