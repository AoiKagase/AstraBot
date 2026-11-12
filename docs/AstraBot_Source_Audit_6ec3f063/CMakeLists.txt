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
