if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required.")
endif()

file(GLOB root_markdown "${ROOT}/*.md")
file(GLOB_RECURSE documentation
    "${ROOT}/docs/*.md"
    "${ROOT}/apps/examples/*.md"
    "${ROOT}/apps/rPlayer/*.md"
    "${ROOT}/apps/SimpleNES/*.md"
    "${ROOT}/validation/*.md")
list(FILTER documentation EXCLUDE REGEX "/docs/internal/terminal-[^/]+/")
list(APPEND documentation ${root_markdown})

foreach(document IN LISTS documentation)
    file(READ "${document}" contents)
    string(REGEX MATCHALL "\\[[^]]*\\]\\([^)]+\\)" links "${contents}")
    get_filename_component(document_directory "${document}" DIRECTORY)
    foreach(link IN LISTS links)
        string(REGEX REPLACE ".*\\]\\(([^)]+)\\).*" "\\1" target "${link}")
        if(target MATCHES "^(https?://|mailto:|#)")
            continue()
        endif()
        string(REGEX REPLACE "#.*$" "" target "${target}")
        if(target STREQUAL "")
            continue()
        endif()
        get_filename_component(resolved "${document_directory}/${target}" ABSOLUTE)
        if(NOT EXISTS "${resolved}")
            file(RELATIVE_PATH relative_document "${ROOT}" "${document}")
            message(FATAL_ERROR "Broken local link in ${relative_document}: ${target}")
        endif()
    endforeach()
endforeach()

file(READ "${ROOT}/README.md" readme)
file(READ "${ROOT}/vcpkg.json" manifest)
file(READ "${ROOT}/include/rasterm/Version.hpp" version_header)
file(READ "${ROOT}/CMakeLists.txt" project_file)

string(REGEX MATCH
    "project\\(rasterm VERSION ([0-9]+)\\.([0-9]+)\\.([0-9]+)"
    project_match "${project_file}")
if(NOT project_match)
    message(FATAL_ERROR "Could not read the rasterm version from CMakeLists.txt.")
endif()

set(project_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
set(project_major "${CMAKE_MATCH_1}")
set(project_minor "${CMAKE_MATCH_2}")
set(project_patch "${CMAKE_MATCH_3}")
string(JSON manifest_version GET "${manifest}" version-semver)

string(FIND "${readme}" "# rasterm ${project_version}" readme_version)
string(FIND "${version_header}" "RASTERM_VERSION_MAJOR ${project_major}" header_major)
string(FIND "${version_header}" "RASTERM_VERSION_MINOR ${project_minor}" header_minor)
string(FIND "${version_header}" "RASTERM_VERSION_PATCH ${project_patch}" header_patch)
string(FIND "${version_header}" "RASTERM_VERSION_PRERELEASE \"\"" header_prerelease)
if(readme_version EQUAL -1 OR
   NOT manifest_version STREQUAL project_version OR
   header_major EQUAL -1 OR
   header_minor EQUAL -1 OR
   header_patch EQUAL -1 OR
   header_prerelease EQUAL -1)
    message(FATAL_ERROR
        "Version mismatch: CMake=${project_version}, vcpkg=${manifest_version}. "
        "Check README.md and include/rasterm/Version.hpp as well.")
endif()
