if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required.")
endif()

file(GLOB_RECURSE first_party_sources
    "${ROOT}/include/rasterm/*.h"
    "${ROOT}/include/rasterm/*.hpp"
    "${ROOT}/src/*.cpp"
    "${ROOT}/src/*.hpp"
    "${ROOT}/apps/rPlayer/*.cpp"
    "${ROOT}/apps/rPlayer/*.hpp"
    "${ROOT}/apps/examples/*.c"
    "${ROOT}/apps/examples/*.cpp"
    "${ROOT}/validation/*.c"
    "${ROOT}/validation/*.cpp"
    "${ROOT}/validation/*.hpp")

foreach(source IN LISTS first_party_sources)
    if(source MATCHES "[/\\\\](build|vcpkg_installed)[/\\\\]")
        continue()
    endif()
    file(STRINGS "${source}" first_line LIMIT_COUNT 1)
    if(NOT first_line STREQUAL "/* SPDX-License-Identifier: Apache-2.0 */")
        message(FATAL_ERROR "Missing Apache-2.0 SPDX identifier: ${source}")
    endif()
endforeach()
