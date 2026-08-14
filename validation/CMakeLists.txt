# SPDX-License-Identifier: Apache-2.0

function(rasterm_add_validation_test target test_name source)
    cmake_parse_arguments(ARG "" "TIMEOUT" "SOURCES;INCLUDES" ${ARGN})
    add_executable(${target} ${source} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE rasterm::rasterm)
    if(ARG_INCLUDES)
        target_include_directories(${target} PRIVATE ${ARG_INCLUDES})
    endif()
    add_test(NAME ${test_name} COMMAND ${target})
    if(ARG_TIMEOUT)
        set_tests_properties(${test_name} PROPERTIES TIMEOUT ${ARG_TIMEOUT})
    endif()
endfunction()

if(RASTERM_BUILD_TESTS)
    file(GLOB RASTERM_INSTALLED_CPP_HEADERS CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/rasterm/*.hpp")
    set(RASTERM_HEADER_CHECK_SOURCES)
    foreach(header IN LISTS RASTERM_INSTALLED_CPP_HEADERS)
        get_filename_component(header_name "${header}" NAME)
        set(generated "${CMAKE_CURRENT_BINARY_DIR}/headers/${header_name}.cpp")
        file(GENERATE OUTPUT "${generated}" CONTENT "#include <rasterm/${header_name}>\n")
        list(APPEND RASTERM_HEADER_CHECK_SOURCES "${generated}")
    endforeach()
    add_library(rasterm-public-header-checks OBJECT ${RASTERM_HEADER_CHECK_SOURCES})
    target_link_libraries(rasterm-public-header-checks PRIVATE rasterm::rasterm)

    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/headers/capi.c"
        CONTENT "#include <rasterm/capi.h>\n")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/headers/defaults.c"
        CONTENT "#include <rasterm/defaults.h>\nint rasterm_defaults_header_check;\n")
    add_library(rasterm-public-c-header-checks OBJECT
        "${CMAKE_CURRENT_BINARY_DIR}/headers/capi.c"
        "${CMAKE_CURRENT_BINARY_DIR}/headers/defaults.c")
    target_link_libraries(rasterm-public-c-header-checks PRIVATE rasterm::rasterm)
    if(MSVC)
        target_compile_options(rasterm-public-c-header-checks PRIVATE /wd4206)
    endif()

    rasterm_add_validation_test(rasterm-smoke rasterm.public-api-smoke tests/smoke.cpp)
    rasterm_add_validation_test(rasterm-public-api-tests rasterm.public-api
        tests/public_api.cpp INCLUDES "${PROJECT_SOURCE_DIR}")
    rasterm_add_validation_test(rasterm-encoder-tests rasterm.encoder-golden-roundtrip
        tests/encoder_golden.cpp INCLUDES "${PROJECT_SOURCE_DIR};${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-quality-fixture-tests rasterm.quality-fixtures
        tests/quality_fixtures.cpp INCLUDES "${PROJECT_SOURCE_DIR};${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-damage-tests rasterm.damage tests/damage.cpp
        INCLUDES "${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-damage-property-tests rasterm.damage-properties
        tests/damage_properties.cpp INCLUDES "${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-stress-tests rasterm.stress tests/stress.cpp TIMEOUT 30)
    rasterm_add_validation_test(rasterm-color-geometry-tests rasterm.color-geometry
        tests/color_geometry.cpp INCLUDES "${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-scaling-tests rasterm.scaling tests/scaling.cpp)
    rasterm_add_validation_test(rasterm-color-conformance-tests rasterm.color-conformance
        tests/color_conformance.cpp INCLUDES "${PROJECT_SOURCE_DIR};${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-metadata-ownership-tests rasterm.metadata-ownership
        tests/metadata_ownership.cpp)
    rasterm_add_validation_test(rasterm-c-api-tests rasterm.c-api tests/c_api.c)
    rasterm_add_validation_test(rasterm-validation-tests rasterm.validation tests/validation.cpp)
    rasterm_add_validation_test(rasterm-defaults-tests rasterm.c-cpp-defaults tests/defaults.cpp)
    rasterm_add_validation_test(rasterm-allocation-failure-tests rasterm.allocation-failure
        tests/allocation_failure.cpp TIMEOUT 30)
    rasterm_add_validation_test(rasterm-presenter-concurrency-tests rasterm.presenter-concurrency
        tests/presenter_concurrency.cpp TIMEOUT 30)
    rasterm_add_validation_test(rasterm-realtime-contract-tests rasterm.realtime-contract
        tests/realtime_contract.cpp TIMEOUT 30)
    rasterm_add_validation_test(rasterm-protocol-recovery-tests rasterm.protocol-recovery
        tests/protocol_recovery.cpp INCLUDES "${PROJECT_SOURCE_DIR};${PROJECT_SOURCE_DIR}/src")
    rasterm_add_validation_test(rasterm-state-machine-tests rasterm.state-machine
        tests/state_machine.cpp)
    rasterm_add_validation_test(rasterm-terminal-owner-tests rasterm.terminal-owner
        tests/terminal_owner.cpp
        INCLUDES "${PROJECT_SOURCE_DIR}/src")

    add_executable(rasterm-abi-tests tests/abi.cpp)
    target_link_libraries(rasterm-abi-tests PRIVATE rasterm::rasterm)
    add_test(NAME rasterm.c-abi-layout COMMAND rasterm-abi-tests
        "${PROJECT_SOURCE_DIR}/validation/tests/abi/c_api_v1.layout")

    if(MSVC)
        get_filename_component(RASTERM_MSVC_TOOL_DIR "${CMAKE_LINKER}" DIRECTORY)
        find_program(RASTERM_DUMPBIN dumpbin HINTS "${RASTERM_MSVC_TOOL_DIR}")
        if(RASTERM_DUMPBIN)
            add_test(NAME rasterm.c-abi-symbols
                COMMAND ${CMAKE_COMMAND}
                    -DDUMPBIN=${RASTERM_DUMPBIN}
                    -DLIBRARY=$<TARGET_FILE:rasterm>
                    -DBASELINE=${PROJECT_SOURCE_DIR}/validation/tests/abi/c_api_v1.symbols
                    -P ${PROJECT_SOURCE_DIR}/cmake/CheckCAbiSymbols.cmake)
            set(RASTERM_DUMPBIN "${RASTERM_DUMPBIN}" PARENT_SCOPE)
        else()
            message(WARNING "dumpbin was not found, the C ABI symbol test is unavailable.")
        endif()
    endif()

    add_test(NAME rasterm.visual-studio-project-parity
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P ${PROJECT_SOURCE_DIR}/cmake/VerifyVisualStudioProject.cmake)
    add_test(NAME rasterm.release-source-contents
        COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
            -P ${PROJECT_SOURCE_DIR}/cmake/VerifyReleaseContents.cmake)
    add_test(NAME rasterm.spdx
        COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
            -P ${PROJECT_SOURCE_DIR}/cmake/VerifySpdx.cmake)
    add_test(NAME rasterm.documentation
        COMMAND ${CMAKE_COMMAND} -DROOT=${PROJECT_SOURCE_DIR}
            -P ${PROJECT_SOURCE_DIR}/cmake/VerifyDocumentation.cmake)

    if(RASTERM_ENABLE_ASAN AND RASTERM_MSVC_FRONTEND)
        get_filename_component(RASTERM_ASAN_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_property(RASTERM_TEST_NAMES DIRECTORY PROPERTY TESTS)
        set_tests_properties(${RASTERM_TEST_NAMES} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${RASTERM_ASAN_RUNTIME_DIR}")
    endif()
endif()

if(RASTERM_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR RASTERM_MSVC_FRONTEND)
        message(FATAL_ERROR
            "RASTERM_BUILD_FUZZERS requires clang/clang++ with Ninja, not ClangCL.")
    endif()
    if(WIN32 AND NOT RASTERM_ENABLE_ASAN)
        message(FATAL_ERROR "Windows libFuzzer targets require RASTERM_ENABLE_ASAN=ON.")
    endif()
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
        OUTPUT_VARIABLE RASTERM_CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE RASTERM_CLANG_RESOURCE_RESULT)
    if(NOT RASTERM_CLANG_RESOURCE_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to locate the Clang compiler runtime directory.")
    endif()
    find_file(RASTERM_ASAN_RUNTIME_DLL NAMES clang_rt.asan_dynamic-x86_64.dll
        HINTS "${RASTERM_CLANG_RESOURCE_DIR}/lib/windows" NO_DEFAULT_PATH REQUIRED)
    target_compile_options(rasterm PRIVATE -fsanitize=fuzzer-no-link)
    foreach(fuzzer IN ITEMS frame_validation damage_rectangles c_structures sixel_writer)
        add_executable(rasterm-fuzz-${fuzzer} tests/fuzz/${fuzzer}.cpp)
        target_link_libraries(rasterm-fuzz-${fuzzer} PRIVATE rasterm::rasterm)
        target_include_directories(rasterm-fuzz-${fuzzer} PRIVATE "${PROJECT_SOURCE_DIR}/src")
        target_compile_options(rasterm-fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer-no-link)
        target_link_options(rasterm-fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer,address)
        set_target_properties(rasterm-fuzz-${fuzzer} PROPERTIES
            MSVC_RUNTIME_LIBRARY "${RASTERM_MSVC_RUNTIME_LIBRARY}")
        add_custom_command(TARGET rasterm-fuzz-${fuzzer} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RASTERM_ASAN_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:rasterm-fuzz-${fuzzer}>" VERBATIM)
    endforeach()
endif()

if(RASTERM_BUILD_BENCHMARKS)
    add_executable(rasterm-encoder-benchmark benchmarks/encoder_benchmark.cpp)
    target_link_libraries(rasterm-encoder-benchmark PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-encoder-benchmark PRIVATE
        "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")

    add_executable(rasterm-encoder-stage-benchmark benchmarks/encoder_stage_benchmark.cpp)
    target_link_libraries(rasterm-encoder-stage-benchmark PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-encoder-stage-benchmark PRIVATE
        "${PROJECT_SOURCE_DIR}/src")

    add_executable(rasterm-palette-lookup-benchmark benchmarks/palette_lookup_benchmark.cpp)
    target_link_libraries(rasterm-palette-lookup-benchmark PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-palette-lookup-benchmark PRIVATE
        "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")

    add_executable(rasterm-damage-surface-benchmark benchmarks/damage_surface_benchmark.cpp)
    target_link_libraries(rasterm-damage-surface-benchmark PRIVATE rasterm::rasterm)
    target_include_directories(rasterm-damage-surface-benchmark PRIVATE
        "${PROJECT_SOURCE_DIR}/src")

    add_executable(rasterm-windows-terminal-sixel-repro
        benchmarks/windows_terminal_sixel_repro.cpp)
    target_link_libraries(rasterm-windows-terminal-sixel-repro PRIVATE rasterm::rasterm)
endif()
