if(NOT DEFINED DUMPBIN OR NOT DEFINED LIBRARY OR NOT DEFINED BASELINE)
    message(FATAL_ERROR "DUMPBIN, LIBRARY, and BASELINE are required")
endif()

if(EXPORTS)
    set(dumpbin_mode /exports)
else()
    set(dumpbin_mode /linkermember:1)
endif()

execute_process(
    COMMAND "${DUMPBIN}" /nologo ${dumpbin_mode} "${LIBRARY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "dumpbin failed: ${error}")
endif()

string(REPLACE "\r\n" "\n" output "${output}")
string(REPLACE "\n" ";" lines "${output}")
set(actual)
foreach(line IN LISTS lines)
    if(line MATCHES "[ |](rasterm_[a-z0-9_]+)$")
        list(APPEND actual "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES actual)
list(SORT actual)
file(STRINGS "${BASELINE}" expected REGEX "^[a-z0-9_]+$")
list(SORT expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "C ABI symbol drift.\nExpected: ${expected}\nActual: ${actual}")
endif()
