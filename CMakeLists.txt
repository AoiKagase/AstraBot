cmake_minimum_required(VERSION 3.20)

project(AstraBot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ASTRABOT_BUILD_TESTS "Build AstraBot portable contract tests" ON)
option(ASTRABOT_BUILD_METAMOD "Build the Metamod-P adapter" OFF)
option(ASTRABOT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(ASTRABOT_NAV_ASAN "Build dedicated Nav evidence tests with MSVC AddressSanitizer" OFF)
option(ASTRABOT_BUILD_NAV_BENCHMARK "Build non-gating Nav microbenchmark" OFF)
option(ASTRABOT_BUILD_NAV_INSPECTOR "Build read-only SDK-free NAV inspector" OFF)
if(ASTRABOT_NAV_ASAN)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR NOT WIN32 OR
       NOT ASTRABOT_BUILD_TESTS OR ASTRABOT_BUILD_METAMOD)
        message(FATAL_ERROR "Nav ASan requires Windows MSVC, tests ON and Metamod OFF")
    endif()
    # ASan is incompatible with /RTC and edit-and-continue (/ZI).
    string(REGEX REPLACE "/RTC[^ ]*" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

function(astrabot_nav_sanitize target)
    if(ASTRABOT_NAV_ASAN)
        target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
        target_link_options(${target} PRIVATE /INCREMENTAL:NO)
    endif()
endfunction()

set(ASTRABOT_METAMOD_SDK_ROOT "" CACHE PATH
    "Pinned external Metamod-P SDK checkout used by the adapter")

function(astrabot_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(NOT (ARGC GREATER 1 AND "${ARGV1}" STREQUAL "LEGACY_SDK"))
            target_compile_options(${target} PRIVATE /permissive-)
        endif()
        if(ASTRABOT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
        )
        if(ASTRABOT_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(astrabot_enable_metamod_compatibility target)
    if(MSVC)
        # The pinned HLSDK predates MSVC's strict string and conformance
        # defaults.  Keep this compatibility scope on adapter targets only.
        target_compile_options(${target} PRIVATE
            /Zc:strictStrings-
            /wd4005
            /wd4302
            /wd4311
            /wd4459
        )
    endif()
endfunction()

add_library(astrabot_core STATIC
    src/core/command.cpp
    src/core/identity.cpp
    src/host/bot_agents.cpp
    src/host/game_host.cpp
    src/host/player_registry.cpp
)

target_compile_features(astrabot_core PUBLIC cxx_std_17)
target_include_directories(astrabot_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(astrabot_core PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
astrabot_enable_warnings(astrabot_core)

add_library(astrabot_nav STATIC
    src/nav/query/route_search.cpp
    src/nav/query/graph.cpp
    src/nav/query/spatial_index.cpp
    src/nav/query/geometry.cpp
    src/nav/io/decode_context.cpp
    src/nav/io/mesh_loader.cpp
    src/nav/model/mesh_validator.cpp
    src/nav/model/value_types.cpp
    src/nav/io/byte_reader.cpp
    src/nav/io/nav_reader.cpp
    src/nav/io/area_reader.cpp
)

target_compile_features(astrabot_nav PUBLIC cxx_std_17)
if(MSVC)
    # Debug STL proxy allocation occurs inside noexcept vector constructors/moves.
    # Disable that instrumentation consistently across Nav's public STL ABI so
    # allocation failures can reach the loader's typed diagnostics.
    target_compile_definitions(astrabot_nav PUBLIC _ITERATOR_DEBUG_LEVEL=0)
endif()
target_include_directories(astrabot_nav
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(astrabot_nav PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
astrabot_enable_warnings(astrabot_nav)
astrabot_nav_sanitize(astrabot_nav)

if(ASTRABOT_BUILD_NAV_BENCHMARK)
    add_executable(astrabot_nav_benchmark tests/nav/evidence_benchmark.cpp)
    target_link_libraries(astrabot_nav_benchmark PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_benchmark)
    astrabot_nav_sanitize(astrabot_nav_benchmark)
endif()

if(ASTRABOT_BUILD_NAV_INSPECTOR)
    add_executable(astrabot_nav_inspect tools/nav-inspect.cpp tools/nav_inspection.cpp)
    target_link_libraries(astrabot_nav_inspect PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_inspect)
    astrabot_nav_sanitize(astrabot_nav_inspect)
endif()

if(ASTRABOT_BUILD_METAMOD)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "ASTRABOT_BUILD_METAMOD requires a 32-bit toolchain for the "
            "GoldSrc/Metamod-P ABI; configure with Win32 or -m32.")
    endif()

    if(NOT ASTRABOT_METAMOD_SDK_ROOT)
        message(FATAL_ERROR
            "ASTRABOT_BUILD_METAMOD=ON requires "
            "-DASTRABOT_METAMOD_SDK_ROOT=<fixed Metamod-P checkout>")
    endif()

    if(NOT IS_DIRECTORY "${ASTRABOT_METAMOD_SDK_ROOT}")
        message(FATAL_ERROR
            "ASTRABOT_METAMOD_SDK_ROOT is not a directory: "
            "${ASTRABOT_METAMOD_SDK_ROOT}")
    endif()

    set(ASTRABOT_METAMOD_REQUIRED_SHA
        "7ec9b014f8c0a947a724644aebe34eb33706e44b")
    execute_process(
        COMMAND git -C "${ASTRABOT_METAMOD_SDK_ROOT}" rev-parse HEAD
        RESULT_VARIABLE _astrabot_metamod_git_result
        OUTPUT_VARIABLE _astrabot_metamod_head
        ERROR_VARIABLE _astrabot_metamod_git_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _astrabot_metamod_git_result EQUAL 0)
        message(FATAL_ERROR
            "ASTRABOT_METAMOD_SDK_ROOT must be a git checkout at "
            "${ASTRABOT_METAMOD_REQUIRED_SHA}; git rev-parse failed: "
            "${_astrabot_metamod_git_error}")
    endif()
    if(NOT _astrabot_metamod_head STREQUAL "${ASTRABOT_METAMOD_REQUIRED_SHA}")
        message(FATAL_ERROR
            "Metamod-P SDK checkout is not pinned to the required SHA. "
            "Expected ${ASTRABOT_METAMOD_REQUIRED_SHA}, got "
            "${_astrabot_metamod_head}.")
    endif()

    set(_astrabot_metamod_sdk_include_dirs
        "${ASTRABOT_METAMOD_SDK_ROOT}/metamod"
        "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk"
        "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/engine"
        "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/common"
        "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/pm_shared"
        "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/dlls"
    )
    foreach(_astrabot_metamod_include_dir IN LISTS
            _astrabot_metamod_sdk_include_dirs)
        if(NOT IS_DIRECTORY "${_astrabot_metamod_include_dir}")
            message(FATAL_ERROR
                "Required Metamod-P SDK include directory is missing: "
                "${_astrabot_metamod_include_dir}")
        endif()
    endforeach()

    set(_astrabot_metamod_required_headers
        "metamod/meta_api.h"
        "metamod/dllapi.h"
        "metamod/engine_api.h"
        "metamod/mutil.h"
        "metamod/plinfo.h"
        "metamod/osdep.h"
    )
    foreach(_astrabot_metamod_header IN LISTS
            _astrabot_metamod_required_headers)
        if(NOT EXISTS "${ASTRABOT_METAMOD_SDK_ROOT}/${_astrabot_metamod_header}")
            message(FATAL_ERROR
                "Required Metamod-P SDK header is missing: "
                "${ASTRABOT_METAMOD_SDK_ROOT}/${_astrabot_metamod_header}")
        endif()
    endforeach()

    add_library(astrabot_mm SHARED
        src/adapter/cstrike/join_state.cpp
        src/adapter/cstrike/messages.cpp
        src/adapter/metamod/fake_client.cpp
        src/adapter/metamod/plugin_entry.cpp
        src/adapter/metamod/lifecycle.cpp
        src/adapter/metamod/movement.cpp
        src/debug/host_trace.cpp
    )
    target_compile_features(astrabot_mm PRIVATE cxx_std_17)
    target_link_libraries(astrabot_mm PRIVATE astrabot_core)
    target_include_directories(astrabot_mm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_include_directories(astrabot_mm SYSTEM PRIVATE
        ${_astrabot_metamod_sdk_include_dirs}
    )
    set_target_properties(astrabot_mm PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
        OUTPUT_NAME "astrabot_mm"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
    )
    if(WIN32)
        set_target_properties(astrabot_mm PROPERTIES SUFFIX ".dll")
    elseif(UNIX)
        set_target_properties(astrabot_mm PROPERTIES SUFFIX ".so")
    endif()
    astrabot_enable_warnings(astrabot_mm LEGACY_SDK)
    astrabot_enable_metamod_compatibility(astrabot_mm)
endif()

if(ASTRABOT_BUILD_TESTS)
    enable_testing()
    if(ASTRABOT_BUILD_NAV_INSPECTOR)
        add_executable(astrabot_nav_inspection_tests tests/nav/inspection_tests.cpp tools/nav_inspection.cpp)
        target_link_libraries(astrabot_nav_inspection_tests PRIVATE astrabot_nav)
        astrabot_enable_warnings(astrabot_nav_inspection_tests)
        astrabot_nav_sanitize(astrabot_nav_inspection_tests)
        add_test(NAME astrabot.nav.inspection COMMAND astrabot_nav_inspection_tests
            "${CMAKE_CURRENT_BINARY_DIR}/inspection-fixtures")
        set_tests_properties(astrabot.nav.inspection PROPERTIES TIMEOUT 60)
        add_test(NAME astrabot.nav.inspection_cli COMMAND ${CMAKE_COMMAND}
            "-DINSPECTOR=$<TARGET_FILE:astrabot_nav_inspect>"
            "-DFIXTURES=$<TARGET_FILE:astrabot_nav_inspection_tests>"
            "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/inspection-cli-fixtures"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/nav/inspection_cli.cmake")
        set_tests_properties(astrabot.nav.inspection_cli PROPERTIES TIMEOUT 60)
        if(WIN32)
            add_test(NAME astrabot.nav.real_checker COMMAND ${CMAKE_COMMAND}
                "-DINSPECTOR=$<TARGET_FILE:astrabot_nav_inspect>"
                "-DFIXTURES=$<TARGET_FILE:astrabot_nav_inspection_tests>"
                "-DCHECKER=${CMAKE_CURRENT_SOURCE_DIR}/tools/check-real-nav.ps1"
                "-DDIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/real-checker-fixtures"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/nav/real_nav_checker.cmake")
            set_tests_properties(astrabot.nav.real_checker PROPERTIES TIMEOUT 60)
        endif()
    endif()
    add_executable(astrabot_nav_corruption_tests tests/nav/corruption_tests.cpp)
    target_link_libraries(astrabot_nav_corruption_tests PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_corruption_tests)
    add_test(NAME astrabot.nav.corruption COMMAND astrabot_nav_corruption_tests)
    set_tests_properties(astrabot.nav.corruption PROPERTIES TIMEOUT 60)
    add_executable(astrabot_nav_fuzz_tests tests/nav/fuzz_tests.cpp)
    target_link_libraries(astrabot_nav_fuzz_tests PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_fuzz_tests)
    add_test(NAME astrabot.nav.fuzz COMMAND astrabot_nav_fuzz_tests)
    set_tests_properties(astrabot.nav.fuzz PROPERTIES TIMEOUT 60)
    add_executable(astrabot_nav_differential_tests tests/nav/differential_tests.cpp)
    target_link_libraries(astrabot_nav_differential_tests PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_differential_tests)
    add_test(NAME astrabot.nav.differential COMMAND astrabot_nav_differential_tests)
    set_tests_properties(astrabot.nav.differential PROPERTIES TIMEOUT 60)
    foreach(evidence_target IN ITEMS astrabot_nav_corruption_tests astrabot_nav_fuzz_tests astrabot_nav_differential_tests)
        astrabot_nav_sanitize(${evidence_target})
    endforeach()
    # Separate test directory: long fuzz does not change the normal CTest count.
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/long-fuzz/CTestTestfile.cmake"
        CONTENT "add_test(astrabot.nav.fuzz.long \"$<TARGET_FILE:astrabot_nav_fuzz_tests>\" --cases 100000)\nset_tests_properties(astrabot.nav.fuzz.long PROPERTIES TIMEOUT 600)\n")
    if(NOT ASTRABOT_NAV_ASAN)
    find_package(Threads REQUIRED)
    add_executable(astrabot_nav_enrichment_tests tests/nav/enrichment_tests.cpp)
    target_link_libraries(astrabot_nav_enrichment_tests PRIVATE astrabot_nav Threads::Threads)
    astrabot_enable_warnings(astrabot_nav_enrichment_tests)
    add_test(NAME astrabot.nav.enrichment COMMAND astrabot_nav_enrichment_tests)
    set_tests_properties(astrabot.nav.enrichment PROPERTIES TIMEOUT 30)
    add_executable(astrabot_nav_route_tests tests/nav/route_search_tests.cpp)
    target_link_libraries(astrabot_nav_route_tests PRIVATE astrabot_nav Threads::Threads)
    astrabot_enable_warnings(astrabot_nav_route_tests)
    add_test(NAME astrabot.nav.route COMMAND astrabot_nav_route_tests)
    set_tests_properties(astrabot.nav.route PROPERTIES TIMEOUT 30)
    add_executable(astrabot_nav_graph_tests tests/nav/graph_tests.cpp)
    target_link_libraries(astrabot_nav_graph_tests PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_graph_tests)
    add_test(NAME astrabot.nav.graph COMMAND astrabot_nav_graph_tests)
    add_executable(astrabot_nav_spatial_tests tests/nav/spatial_index_tests.cpp)
    target_link_libraries(astrabot_nav_spatial_tests PRIVATE astrabot_nav Threads::Threads)
    astrabot_enable_warnings(astrabot_nav_spatial_tests)
    add_test(NAME astrabot.nav.spatial COMMAND astrabot_nav_spatial_tests)
    add_executable(astrabot_nav_geometry_tests tests/nav/geometry_tests.cpp)
    target_link_libraries(astrabot_nav_geometry_tests PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_geometry_tests)
    add_test(NAME astrabot.nav.geometry COMMAND astrabot_nav_geometry_tests)
    add_executable(astrabot_nav_mesh_tests tests/nav/mesh_loader_tests.cpp)
    target_link_libraries(astrabot_nav_mesh_tests PRIVATE astrabot_nav)
    astrabot_enable_warnings(astrabot_nav_mesh_tests)
    add_test(NAME astrabot.nav.mesh COMMAND astrabot_nav_mesh_tests)

    add_executable(astrabot_portable_contract_tests
        tests/portable_contracts.cpp
    )
    target_link_libraries(astrabot_portable_contract_tests PRIVATE astrabot_core)
    target_compile_features(astrabot_portable_contract_tests PRIVATE cxx_std_17)
    set_target_properties(astrabot_portable_contract_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_portable_contract_tests)

    add_test(
        NAME astrabot.portable.contracts
        COMMAND astrabot_portable_contract_tests
    )

    add_executable(astrabot_player_registry_tests
        tests/host/player_registry_tests.cpp
    )
    target_link_libraries(astrabot_player_registry_tests PRIVATE astrabot_core)
    target_compile_features(astrabot_player_registry_tests PRIVATE cxx_std_17)
    set_target_properties(astrabot_player_registry_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_player_registry_tests)

    add_test(
        NAME astrabot.player_registry
        COMMAND astrabot_player_registry_tests
    )

    add_executable(astrabot_bot_agents_tests
        tests/host/bot_agents_tests.cpp
    )
    target_link_libraries(astrabot_bot_agents_tests PRIVATE astrabot_core)
    target_compile_features(astrabot_bot_agents_tests PRIVATE cxx_std_17)
    set_target_properties(astrabot_bot_agents_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_bot_agents_tests)

    add_test(
        NAME astrabot.bot_agents
        COMMAND astrabot_bot_agents_tests
    )

    add_executable(astrabot_nav_value_tests
        tests/nav/value_types_tests.cpp
    )
    target_link_libraries(astrabot_nav_value_tests PRIVATE astrabot_nav)
    target_compile_features(astrabot_nav_value_tests PRIVATE cxx_std_17)
    target_include_directories(astrabot_nav_value_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(astrabot_nav_value_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_nav_value_tests)

    add_test(
        NAME astrabot.nav.values
        COMMAND astrabot_nav_value_tests
    )

    add_executable(astrabot_nav_byte_reader_tests
        tests/nav/byte_reader_tests.cpp
    )
    target_link_libraries(astrabot_nav_byte_reader_tests PRIVATE astrabot_nav)
    target_compile_features(astrabot_nav_byte_reader_tests PRIVATE cxx_std_17)
    target_include_directories(astrabot_nav_byte_reader_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(astrabot_nav_byte_reader_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_nav_byte_reader_tests)

    add_test(
        NAME astrabot.nav.byte_reader
        COMMAND astrabot_nav_byte_reader_tests
    )

    add_executable(astrabot_nav_reader_tests
        tests/nav/nav_reader_tests.cpp
    )
    target_link_libraries(astrabot_nav_reader_tests PRIVATE astrabot_nav)
    target_compile_features(astrabot_nav_reader_tests PRIVATE cxx_std_17)
    target_include_directories(astrabot_nav_reader_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(astrabot_nav_reader_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_nav_reader_tests)

    add_test(
        NAME astrabot.nav.reader
        COMMAND astrabot_nav_reader_tests
    )

    add_executable(astrabot_nav_area_tests
        tests/nav/area_reader_tests.cpp
    )
    target_link_libraries(astrabot_nav_area_tests PRIVATE astrabot_nav)
    target_compile_features(astrabot_nav_area_tests PRIVATE cxx_std_17)
    target_include_directories(astrabot_nav_area_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_target_properties(astrabot_nav_area_tests PROPERTIES
        CXX_EXTENSIONS OFF
    )
    astrabot_enable_warnings(astrabot_nav_area_tests)

    add_test(
        NAME astrabot.nav.area_records
        COMMAND astrabot_nav_area_tests
    )

    if(ASTRABOT_BUILD_METAMOD)
        add_executable(astrabot_adapter_entry_tests
            tests/adapter/plugin_entry_tests.cpp
            src/adapter/cstrike/join_state.cpp
            src/adapter/cstrike/messages.cpp
            src/adapter/metamod/fake_client.cpp
            src/adapter/metamod/plugin_entry.cpp
            src/adapter/metamod/lifecycle.cpp
            src/adapter/metamod/movement.cpp
            src/debug/host_trace.cpp
        )
        target_compile_features(astrabot_adapter_entry_tests PRIVATE cxx_std_17)
        target_link_libraries(astrabot_adapter_entry_tests PRIVATE astrabot_core)
        target_include_directories(astrabot_adapter_entry_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_include_directories(astrabot_adapter_entry_tests SYSTEM PRIVATE
            ${_astrabot_metamod_sdk_include_dirs}
        )
        set_target_properties(astrabot_adapter_entry_tests PROPERTIES
            CXX_EXTENSIONS OFF
        )
        astrabot_enable_warnings(astrabot_adapter_entry_tests LEGACY_SDK)
        astrabot_enable_metamod_compatibility(astrabot_adapter_entry_tests)

        add_test(
            NAME astrabot.adapter.entry
            COMMAND astrabot_adapter_entry_tests
        )

        add_executable(astrabot_movement_tests
            tests/adapter/movement_tests.cpp
            src/adapter/metamod/movement.cpp
            src/debug/host_trace.cpp
        )
        target_compile_features(astrabot_movement_tests PRIVATE cxx_std_17)
        target_link_libraries(astrabot_movement_tests PRIVATE astrabot_core)
        target_include_directories(astrabot_movement_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_include_directories(astrabot_movement_tests SYSTEM PRIVATE
            ${_astrabot_metamod_sdk_include_dirs}
        )
        set_target_properties(astrabot_movement_tests PROPERTIES
            CXX_EXTENSIONS OFF
        )
        astrabot_enable_warnings(astrabot_movement_tests LEGACY_SDK)
        astrabot_enable_metamod_compatibility(astrabot_movement_tests)

        add_test(
            NAME astrabot.adapter.movement
            COMMAND astrabot_movement_tests
        )

        add_executable(astrabot_fake_client_tests
            tests/adapter/fake_client_tests.cpp
            src/adapter/cstrike/join_state.cpp
            src/adapter/cstrike/messages.cpp
            src/adapter/metamod/fake_client.cpp
            src/adapter/metamod/lifecycle.cpp
            src/adapter/metamod/movement.cpp
            src/adapter/metamod/plugin_entry.cpp
            src/debug/host_trace.cpp
        )
        target_compile_features(astrabot_fake_client_tests PRIVATE cxx_std_17)
        target_link_libraries(astrabot_fake_client_tests PRIVATE astrabot_core)
        target_include_directories(astrabot_fake_client_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_include_directories(astrabot_fake_client_tests SYSTEM PRIVATE
            ${_astrabot_metamod_sdk_include_dirs}
        )
        set_target_properties(astrabot_fake_client_tests PROPERTIES
            CXX_EXTENSIONS OFF
        )
        astrabot_enable_warnings(astrabot_fake_client_tests LEGACY_SDK)
        astrabot_enable_metamod_compatibility(astrabot_fake_client_tests)

        add_test(
            NAME astrabot.fake_client
            COMMAND astrabot_fake_client_tests
        )

        add_executable(astrabot_cstrike_join_tests
            tests/adapter/join_state_tests.cpp
            src/adapter/cstrike/join_state.cpp
            src/adapter/cstrike/messages.cpp
        )
        target_compile_features(astrabot_cstrike_join_tests PRIVATE cxx_std_17)
        target_include_directories(astrabot_cstrike_join_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        set_target_properties(astrabot_cstrike_join_tests PROPERTIES
            CXX_EXTENSIONS OFF
        )
        astrabot_enable_warnings(astrabot_cstrike_join_tests)

        add_test(
            NAME astrabot.cstrike.join
            COMMAND astrabot_cstrike_join_tests
        )
    endif()
    endif() # Normal tests include allocator overrides; dedicated ASan tests do not.
endif()
