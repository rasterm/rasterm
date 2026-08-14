cmake_minimum_required(VERSION 3.24)
project(rasterm VERSION 1.3.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "rasterm currently supports Windows Terminal only.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/rastermSources.cmake)

option(RASTERM_BUILD_TESTS "Build the rasterm validation kit" OFF)
option(RASTERM_BUILD_BENCHMARKS "Build rasterm benchmark tools" OFF)
option(RASTERM_BUILD_FUZZERS "Build libFuzzer targets (GNU-style Clang driver only)" OFF)
option(RASTERM_BUILD_SHARED_C_API "Build the optional shared C ABI library" OFF)
option(RASTERM_ENABLE_ASAN "Enable AddressSanitizer for first party targets" OFF)
option(RASTERM_WARNINGS_AS_ERRORS "Treat first party compiler warnings as errors" ON)

set(RASTERM_MSVC_FRONTEND OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
   CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(RASTERM_MSVC_FRONTEND ON)
endif()

set(RASTERM_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(RASTERM_BUILD_FUZZERS)
    # LLVM's Windows libFuzzer runtime requires the static MSVC runtime.
    set(RASTERM_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(RASTERM_MSVC_FRONTEND)
    add_compile_options(/W4 /permissive-)
    if(RASTERM_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(RASTERM_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
endif()

if(RASTERM_ENABLE_ASAN)
    if(RASTERM_MSVC_FRONTEND)
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    else()
        message(FATAL_ERROR "RASTERM_ENABLE_ASAN is unsupported by this compiler.")
    endif()
endif()

add_library(rasterm STATIC ${RASTERM_CORE_SOURCES})
add_library(rasterm::rasterm ALIAS rasterm)

target_compile_features(rasterm PUBLIC cxx_std_20)
target_compile_definitions(rasterm PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(rasterm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(rasterm PROPERTIES
    OUTPUT_NAME rasterm
    DEBUG_POSTFIX d
    MSVC_RUNTIME_LIBRARY "${RASTERM_MSVC_RUNTIME_LIBRARY}"
)

if(RASTERM_MSVC_FRONTEND)
    if(MSVC)
        target_compile_options(rasterm PRIVATE /MP)
    endif()
    set_source_files_properties(${RASTERM_SIXEL_AVX2_SOURCE} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    set_source_files_properties(${RASTERM_SIXEL_AVX512_SOURCE} PROPERTIES COMPILE_OPTIONS "/arch:AVX512")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rasterm PRIVATE -Wall -Wextra -Wpedantic)
    set_source_files_properties(${RASTERM_SIXEL_AVX2_SOURCE} PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(${RASTERM_SIXEL_AVX512_SOURCE} PROPERTIES COMPILE_OPTIONS "-mavx512f")
endif()

if(MINGW)

    # avoid a libstdc++ DLL ABI dependency while sharing the process wide winpthread runtime used by MinGW hosts such as RetroArch.
    
    target_link_options(rasterm INTERFACE -static-libstdc++)
endif()

if(RASTERM_BUILD_TESTS)
    enable_testing()
endif()

if(RASTERM_BUILD_TESTS OR RASTERM_BUILD_FUZZERS OR RASTERM_BUILD_BENCHMARKS)
    add_subdirectory(validation)
endif()

if(RASTERM_BUILD_SHARED_C_API)
    add_library(rasterm-shared SHARED ${RASTERM_CORE_SOURCES})
    add_library(rasterm::shared ALIAS rasterm-shared)
    target_compile_features(rasterm-shared PUBLIC cxx_std_20)
    target_compile_definitions(rasterm-shared
        PUBLIC RASTERM_SHARED_LIBRARY
        PRIVATE RASTERM_BUILD_SHARED_LIBRARY NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(rasterm-shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(rasterm-shared PROPERTIES
        EXPORT_NAME shared
        OUTPUT_NAME rasterm
        ARCHIVE_OUTPUT_NAME rasterm-import
        PDB_NAME rasterm-shared
        COMPILE_PDB_NAME rasterm-shared-objects
        DEBUG_POSTFIX d
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    if(RASTERM_MSVC_FRONTEND AND MSVC)
        target_compile_options(rasterm-shared PRIVATE /MP)
    elseif(MINGW)
        target_link_options(rasterm-shared PRIVATE -static-libstdc++)
    endif()
    if(RASTERM_BUILD_TESTS AND MSVC AND RASTERM_DUMPBIN)
        add_test(NAME rasterm.c-abi-shared-symbols
            COMMAND ${CMAKE_COMMAND}
                -DDUMPBIN=${RASTERM_DUMPBIN}
                -DLIBRARY=$<TARGET_FILE:rasterm-shared>
                -DBASELINE=${CMAKE_CURRENT_SOURCE_DIR}/validation/tests/abi/c_api_v1.symbols
                -DEXPORTS=ON
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCAbiSymbols.cmake)
    endif()
endif()

install(TARGETS rasterm
    EXPORT rastermTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT RastermCore
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT RastermCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT RastermCore
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(RASTERM_BUILD_SHARED_C_API)
    install(TARGETS rasterm-shared
        EXPORT rastermTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT RastermBindings
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT RastermBindings
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT RastermBindings
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(DIRECTORY include/rasterm
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT RastermCore)
install(EXPORT rastermTargets
    FILE rastermTargets.cmake
    NAMESPACE rasterm::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rasterm
    COMPONENT RastermPackageMetadata
)

configure_package_config_file(
    cmake/rastermConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rastermConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rasterm
)
configure_file(cmake/rasterm.pc.in ${CMAKE_CURRENT_BINARY_DIR}/rasterm.pc @ONLY)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rastermConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/rastermConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/rastermConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rasterm
    COMPONENT RastermPackageMetadata
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rasterm.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT RastermPackageMetadata
)
install(FILES
    LICENSE
    NOTICE
    THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/rasterm
    COMPONENT RastermCore
)

set(CPACK_PACKAGE_NAME rasterm)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "rasterm-${PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR ZIP)
set(CPACK_SOURCE_GENERATOR ZIP)
set(CPACK_COMPONENTS_ALL RastermCore)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_SOURCE_IGNORE_FILES
    "/build/;/vcpkg_installed/;/.git/;/.vs/;/_CPack_Packages/;/apps/retroarch/;/apps/SimpleNES/;/bindings/rust/target/;/bindings/python/build/;/bindings/python/src/[^/]*[.]egg-info/;/rasterm-[^/]*[.]zip$"
)
include(CPack)
