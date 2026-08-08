cmake_minimum_required(VERSION 3.24)
project(rasterm_pkgconfig_consumers LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RASTERM REQUIRED IMPORTED_TARGET rasterm)

add_executable(rasterm-pkgconfig-c c/main.c)
target_compile_features(rasterm-pkgconfig-c PRIVATE c_std_11)
target_link_libraries(rasterm-pkgconfig-c PRIVATE PkgConfig::RASTERM)

add_executable(rasterm-pkgconfig-cpp cpp/main.cpp)
target_compile_features(rasterm-pkgconfig-cpp PRIVATE cxx_std_20)
target_link_libraries(rasterm-pkgconfig-cpp PRIVATE PkgConfig::RASTERM)
