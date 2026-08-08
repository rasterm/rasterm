if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required.")
endif()

if(INSTALL_TREE)
    set(required
        include/rasterm/rasterm.hpp
        include/rasterm/capi.h
        lib/cmake/rasterm/rastermConfig.cmake
        lib/cmake/rasterm/rastermConfigVersion.cmake
        lib/cmake/rasterm/rastermTargets.cmake
        lib/pkgconfig/rasterm.pc
        share/rasterm/LICENSE
        share/rasterm/NOTICE
        share/rasterm/THIRD_PARTY_NOTICES.md)
else()
    set(required
        CMakeLists.txt
        cmake/rastermSources.cmake
        cmake/rastermConfig.cmake.in
        include/rasterm/rasterm.hpp
        include/rasterm/capi.h
        LICENSE
        NOTICE
        THIRD_PARTY_NOTICES.md
        vcpkg.json)
endif()

foreach(path IN LISTS required)
    if(NOT EXISTS "${ROOT}/${path}")
        message(FATAL_ERROR "Release content is missing: ${path}")
    endif()
endforeach()