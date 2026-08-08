if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required.")
endif()

file(STRINGS "${SOURCE_ROOT}/cmake/rastermSources.cmake" cmake_lines
    REGEX "^[ \t]+src/.+\\.cpp$")
set(cmake_sources)
foreach(line IN LISTS cmake_lines)
    string(STRIP "${line}" line)
    list(APPEND cmake_sources "${line}")
endforeach()

file(READ "${SOURCE_ROOT}/rasterm.vcxproj" project)
file(READ "${SOURCE_ROOT}/CMakeLists.txt" cmake_project)
string(REGEX MATCHALL "<ClCompile Include=\"src\\\\[^\"]+\\.cpp\"" project_entries "${project}")
set(project_sources)
foreach(entry IN LISTS project_entries)
    string(REGEX REPLACE ".*Include=\"([^\"]+)\".*" "\\1" source "${entry}")
    string(REPLACE "\\" "/" source "${source}")
    list(APPEND project_sources "${source}")
endforeach()

foreach(required IN ITEMS
        "target_compile_definitions(rasterm PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)"
        "MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>DLL\"")
    string(FIND "${cmake_project}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "CMakeLists.txt is missing required setting: ${required}")
    endif()
endforeach()

list(SORT cmake_sources)
list(SORT project_sources)
if(NOT cmake_sources STREQUAL project_sources)
    message(FATAL_ERROR
        "CMake/MSBuild source lists differ.\nCMake: ${cmake_sources}\nMSBuild: ${project_sources}")
endif()

foreach(required IN ITEMS
        "<WarningLevel>Level4</WarningLevel>"
        "<TreatWarningAsError>true</TreatWarningAsError>"
        "<ConformanceMode>true</ConformanceMode>"
        "<LanguageStandard>stdcpp20</LanguageStandard>"
        "NOMINMAX;WIN32_LEAN_AND_MEAN"
        "MultiThreadedDebugDLL"
        "MultiThreadedDLL")
    string(FIND "${project}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "rasterm.vcxproj is missing required setting: ${required}")
    endif()
endforeach()
