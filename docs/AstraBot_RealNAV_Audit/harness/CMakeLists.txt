cmake_minimum_required(VERSION 3.16)
project(AstraBotNavigationAudit LANGUAGES CXX)

# This standalone project does not alter AstraBot's official x86-only CMake.
set(ASTRABOT_SOURCE_ROOT "" CACHE PATH "Root of the separately extracted AstraBot source")
if(NOT EXISTS "${ASTRABOT_SOURCE_ROOT}/include/astrabot/runtime/nav_roam_controller.hpp")
    message(FATAL_ERROR "Set ASTRABOT_SOURCE_ROOT to the extracted AstraBot source root")
endif()
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NAV_SOURCES
 src/core/compat/random_source.cpp
 src/core/nav/legacy_nav_reader.cpp
    src/adapter/metamod/nav_loader.cpp
    src/adapter/metamod/map_fingerprint.cpp
    src/core/nav/nav_model.cpp
    src/core/nav/nav_snapshot.cpp
    src/core/nav/nav_query.cpp
    src/core/nav/locomotion.cpp
    src/core/nav/jump_drop.cpp
    src/core/nav/special_traversal.cpp
    src/core/runtime/nav_roam_controller.cpp
    src/core/compat/runtime_mode_policy.cpp
    src/core/world/world_snapshot.cpp
)
list(TRANSFORM NAV_SOURCES PREPEND "${ASTRABOT_SOURCE_ROOT}/")
add_library(audit_nav STATIC ${NAV_SOURCES})
target_include_directories(audit_nav PUBLIC "${ASTRABOT_SOURCE_ROOT}/include")
add_executable(audit_repro audit_repro.cpp)
target_link_libraries(audit_repro PRIVATE audit_nav)

enable_testing()
foreach(test_name nav_model nav_snapshot nav_query locomotion jump_drop special_traversal nav_roam_controller)
    add_executable(audit_existing_${test_name} "${ASTRABOT_SOURCE_ROOT}/tests/${test_name}_tests.cpp")
    target_link_libraries(audit_existing_${test_name} PRIVATE audit_nav)
    add_test(NAME existing_${test_name} COMMAND audit_existing_${test_name})
endforeach()
add_test(NAME audit_diagnostic_observations COMMAND audit_repro)
# Intentionally RED on the supplied snapshot. Do not mark WILL_FAIL or disable.
add_test(NAME audit_core_safety COMMAND audit_repro --assert-core-safety)

add_executable(real_nav_probe real_nav_probe.cpp)
target_link_libraries(real_nav_probe PRIVATE audit_nav)
add_executable(official_external_nav "${ASTRABOT_SOURCE_ROOT}/tests/nav_external_performance_tests.cpp")
target_link_libraries(official_external_nav PRIVATE audit_nav)
add_executable(edge_diagnostics edge_diagnostics.cpp)
target_link_libraries(edge_diagnostics PRIVATE audit_nav)
set(ASTRABOT_REAL_NAV "" CACHE FILEPATH "External real NAV; never copied into the source tree")
if(EXISTS "${ASTRABOT_REAL_NAV}")
 add_executable(real_nav_regressions real_nav_regressions.cpp "${ASTRABOT_SOURCE_ROOT}/src/adapter/metamod/action_adapter.cpp")
 target_include_directories(real_nav_regressions PRIVATE "${ASTRABOT_SOURCE_ROOT}/src/adapter/metamod" "${ASTRABOT_SOURCE_ROOT}/third_party/cssdk/common")
 target_link_libraries(real_nav_regressions PRIVATE audit_nav)
 add_test(NAME real_nav_parse_and_dump COMMAND real_nav_probe "${ASTRABOT_REAL_NAV}" "${CMAKE_CURRENT_BINARY_DIR}/nav_graph.json")
 add_test(NAME real_nav_edge_diagnostics COMMAND edge_diagnostics "${ASTRABOT_REAL_NAV}" "${CMAKE_CURRENT_BINARY_DIR}/edge_scan.csv")
 foreach(case baseline slope_entry slope_exit jump_gate gap_policy recovery_projection goal_floor)
  add_test(NAME real_nav_${case} COMMAND real_nav_regressions "${ASTRABOT_REAL_NAV}" "${case}")
 endforeach()
endif()
