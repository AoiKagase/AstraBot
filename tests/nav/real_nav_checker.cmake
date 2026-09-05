# SPDX-License-Identifier: MPL-2.0
# Synthetic exercise of the opt-in checker, never uses installed game assets.
execute_process(COMMAND "${FIXTURES}" "${DIRECTORY}" RESULT_VARIABLE result TIMEOUT 30)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "fixture setup failed")
endif()
string(REPEAT "x" 123 bsp)
file(WRITE "${DIRECTORY}/oracle.bsp" "${bsp}")
function(check_case expected nav goal)
    execute_process(COMMAND powershell -NoProfile -File "${CHECKER}"
        -NavPath "${DIRECTORY}/${nav}" -BspPath "${DIRECTORY}/oracle.bsp"
        -Inspector "${INSPECTOR}" -GoalArea "${goal}" -OutputDirectory "${DIRECTORY}/reports"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if(expected STREQUAL "pass")
        if(NOT result EQUAL 0 OR NOT output MATCHES "OracleCost" OR error)
            message(FATAL_ERROR "independent checker failed: ${output} ${error}")
        endif()
    elseif(NOT "${result}" STREQUAL "1")
        message(FATAL_ERROR "independent checker accepted bad input: ${output} ${error}")
    endif()
endfunction()
check_case(pass v5.nav 2)
check_case(fail v5.nav 999)
check_case(fail v1.nav 2)
file(WRITE "${DIRECTORY}/oracle.bsp" "bad size")
check_case(fail v5.nav 2)
