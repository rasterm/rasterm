cmake_minimum_required(VERSION 3.24)
project(rasterm VERSION 1.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "rasterm currently supports Windows Terminal only.")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/rastermSources.cmake)

option(RASTERM_BUILD_EXAMPLES "Build rasterm examples" OFF)
option(RASTERM_BUILD_TESTS "Build rasterm smoke tests" OFF)
option(RASTERM_BUILD_BENCHMARKS "Build rasterm microbenchmarks" OFF)
option(RASTERM_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
option(RASTERM_BUILD_RASTERMUI "Build the experimental rastermUI library" OFF)
option(RASTERM_BUILD_SHARED_C_API "Build the optional shared C ABI library" OFF)
option(RASTERM_ENABLE_ASAN "Enable AddressSanitizer for first party targets" OFF)
option(RASTERM_WARNINGS_AS_ERRORS "Treat first party compiler warnings as errors" ON)

set(RASTERM_MSVC_FRONTEND OFF)
if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(RASTERM_MSVC_FRONTEND ON)
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
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

if(RASTERM_MSVC_FRONTEND)
    if(MSVC)
        target_compile_options(rasterm PRIVATE /MP)
    endif()
    set_source_files_properties(src/encoder/SixelSimdAvx2.cpp PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rasterm PRIVATE -Wall -Wextra -Wpedantic)
    set_source_files_properties(src/encoder/SixelSimdAvx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
endif()

if(MINGW)

    # avoid a libstdc++ DLL ABI dependency while sharing the process wide winpthread runtime used by MinGW hosts such as RetroArch.
    
    target_link_options(rasterm INTERFACE -static-libstdc++)
endif()

if(RASTERM_BUILD_EXAMPLES)
    add_executable(rasterm-example-c apps/examples/c_api.c)
    add_executable(rasterm-example-synchronous apps/examples/synchronous.cpp)
    add_executable(rasterm-example-presenter apps/examples/presenter.cpp)
    add_executable(rasterm-example-indexed apps/examples/indexed.cpp)
    add_executable(rasterm-example-custom-sink apps/examples/custom_sink.cpp)
    add_executable(rasterm-example-damage apps/examples/damage_rectangles.cpp)
    add_executable(rasterm-example-color apps/examples/color_metadata.cpp)
    foreach(example IN ITEMS
            rasterm-example-c rasterm-example-synchronous rasterm-example-presenter
            rasterm-example-indexed rasterm-example-custom-sink rasterm-example-damage
            rasterm-example-color)
        target_link_libraries(${example} PRIVATE rasterm::rasterm)
    endforeach()
endif()

if(RASTERM_BUILD_TESTS)
    enable_testing()
    add_executable(rasterm-smoke validation/tests/smoke.cpp)
    target_link_libraries(rasterm-smoke PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.public-api-smoke COMMAND rasterm-smoke)

    add_executable(rasterm-public-api-tests validation/tests/public_api.cpp)
    target_link_libraries(rasterm-public-api-tests PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-public-api-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME rasterm.public-api COMMAND rasterm-public-api-tests)

    add_executable(rasterm-encoder-tests validation/tests/encoder_golden.cpp)
    target_link_libraries(rasterm-encoder-tests PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-encoder-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME rasterm.encoder-golden-roundtrip COMMAND rasterm-encoder-tests)

    add_executable(rasterm-damage-tests validation/tests/damage.cpp)
    target_link_libraries(rasterm-damage-tests PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-damage-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME rasterm.damage COMMAND rasterm-damage-tests)

    add_executable(rasterm-stress-tests validation/tests/stress.cpp)
    target_link_libraries(rasterm-stress-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.stress COMMAND rasterm-stress-tests)
    set_tests_properties(rasterm.stress PROPERTIES TIMEOUT 30)

    add_executable(rasterm-adaptive-resolution-tests
        validation/tests/adaptive_resolution.cpp
        apps/rPlayer/src/AdaptiveResolution.cpp
    )
    target_include_directories(rasterm-adaptive-resolution-tests PRIVATE apps/rPlayer/include)
    target_compile_features(rasterm-adaptive-resolution-tests PRIVATE cxx_std_20)
    add_test(NAME rasterm.adaptive-resolution COMMAND rasterm-adaptive-resolution-tests)

    add_executable(rasterm-color-geometry-tests validation/tests/color_geometry.cpp)
    target_link_libraries(rasterm-color-geometry-tests PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-color-geometry-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME rasterm.color-geometry COMMAND rasterm-color-geometry-tests)

    add_executable(rasterm-color-conformance-tests validation/tests/color_conformance.cpp)
    target_link_libraries(rasterm-color-conformance-tests PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-color-conformance-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME rasterm.color-conformance COMMAND rasterm-color-conformance-tests)

    add_executable(rasterm-metadata-ownership-tests validation/tests/metadata_ownership.cpp)
    target_link_libraries(rasterm-metadata-ownership-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.metadata-ownership COMMAND rasterm-metadata-ownership-tests)

    add_executable(rasterm-c-api-tests validation/tests/c_api.c)
    target_link_libraries(rasterm-c-api-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.c-api COMMAND rasterm-c-api-tests)

    add_executable(rasterm-abi-tests validation/tests/abi.cpp)
    target_link_libraries(rasterm-abi-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.c-abi-layout COMMAND rasterm-abi-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/validation/tests/abi/c_api_v1.layout)

    if(MSVC)
        get_filename_component(RASTERM_MSVC_TOOL_DIR "${CMAKE_LINKER}" DIRECTORY)
        find_program(RASTERM_DUMPBIN dumpbin HINTS "${RASTERM_MSVC_TOOL_DIR}")
        if(RASTERM_DUMPBIN)
            add_test(NAME rasterm.c-abi-symbols
                COMMAND ${CMAKE_COMMAND}
                    -DDUMPBIN=${RASTERM_DUMPBIN}
                    -DLIBRARY=$<TARGET_FILE:rasterm>
                    -DBASELINE=${CMAKE_CURRENT_SOURCE_DIR}/validation/tests/abi/c_api_v1.symbols
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCAbiSymbols.cmake)
        else()
            message(WARNING "dumpbin was not found; the C ABI symbol test is unavailable.")
        endif()
    endif()

    add_executable(rasterm-validation-tests validation/tests/validation.cpp)
    target_link_libraries(rasterm-validation-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.validation COMMAND rasterm-validation-tests)

    add_executable(rasterm-defaults-tests validation/tests/defaults.cpp)
    target_link_libraries(rasterm-defaults-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.c-cpp-defaults COMMAND rasterm-defaults-tests)

    add_executable(rasterm-allocation-failure-tests validation/tests/allocation_failure.cpp)
    target_link_libraries(rasterm-allocation-failure-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.allocation-failure COMMAND rasterm-allocation-failure-tests)
    set_tests_properties(rasterm.allocation-failure PROPERTIES TIMEOUT 30)

    add_executable(rasterm-presenter-concurrency-tests validation/tests/presenter_concurrency.cpp)
    target_link_libraries(rasterm-presenter-concurrency-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.presenter-concurrency COMMAND rasterm-presenter-concurrency-tests)
    set_tests_properties(rasterm.presenter-concurrency PROPERTIES TIMEOUT 30)

    add_executable(rasterm-realtime-contract-tests validation/tests/realtime_contract.cpp)
    target_link_libraries(rasterm-realtime-contract-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.realtime-contract COMMAND rasterm-realtime-contract-tests)
    set_tests_properties(rasterm.realtime-contract PROPERTIES TIMEOUT 30)

    add_executable(rasterm-protocol-recovery-tests validation/tests/protocol_recovery.cpp)
    target_link_libraries(rasterm-protocol-recovery-tests PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-protocol-recovery-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME rasterm.protocol-recovery COMMAND rasterm-protocol-recovery-tests)

    add_executable(rasterm-terminal-owner-tests
        validation/tests/terminal_owner.cpp
        src/platform/windows/TerminalOwner.cpp
    )
    target_include_directories(rasterm-terminal-owner-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_features(rasterm-terminal-owner-tests PRIVATE cxx_std_20)
    add_test(NAME rasterm.terminal-owner COMMAND rasterm-terminal-owner-tests)

    add_test(NAME rasterm.visual-studio-project-parity
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyVisualStudioProject.cmake)
    add_test(NAME rasterm.release-source-contents
        COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyReleaseContents.cmake)
    add_test(NAME rasterm.spdx
        COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifySpdx.cmake)
    add_test(NAME rasterm.documentation
        COMMAND ${CMAKE_COMMAND}
            -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyDocumentation.cmake)
endif()

if(RASTERM_BUILD_TESTS AND RASTERM_ENABLE_ASAN AND RASTERM_MSVC_FRONTEND)
    get_filename_component(RASTERM_ASAN_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_property(RASTERM_TEST_NAMES DIRECTORY PROPERTY TESTS)
    set_tests_properties(${RASTERM_TEST_NAMES} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RASTERM_ASAN_RUNTIME_DIR}")
endif()

if(RASTERM_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "RASTERM_BUILD_FUZZERS requires Clang with libFuzzer support.")
    endif()
    foreach(fuzzer IN ITEMS frame_validation damage_rectangles c_structures sixel_writer)
        add_executable(rasterm-fuzz-${fuzzer} validation/tests/fuzz/${fuzzer}.cpp)
        target_link_libraries(rasterm-fuzz-${fuzzer} PRIVATE rasterm::rasterm)
        target_include_directories(rasterm-fuzz-${fuzzer} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_options(rasterm-fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(rasterm-fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer,address)
    endforeach()
endif()

if(RASTERM_BUILD_BENCHMARKS)
    add_executable(rasterm-encoder-benchmark validation/benchmarks/encoder_benchmark.cpp)
    target_link_libraries(rasterm-encoder-benchmark PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-encoder-benchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
endif()

if(RASTERM_BUILD_RASTERMUI)
    add_subdirectory(apps/rastermUI)
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
