cmake_minimum_required(VERSION 3.20)

project(AstraBot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ASTRABOT_BUILD_TESTS "Build AstraBot portable contract tests" ON)
option(ASTRABOT_BUILD_METAMOD "Build the Metamod-P adapter" OFF)
option(ASTRABOT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

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
        src/adapter/metamod/fake_client.cpp
        src/adapter/metamod/plugin_entry.cpp
        src/adapter/metamod/lifecycle.cpp
        src/debug/host_trace.cpp
    )
    target_compile_features(astrabot_mm PRIVATE cxx_std_17)
    target_link_libraries(astrabot_mm PRIVATE astrabot_core)
    target_include_directories(astrabot_mm
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        SYSTEM PRIVATE
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

    if(ASTRABOT_BUILD_METAMOD)
        add_executable(astrabot_adapter_entry_tests
            tests/adapter/plugin_entry_tests.cpp
            src/adapter/metamod/fake_client.cpp
            src/adapter/metamod/plugin_entry.cpp
            src/adapter/metamod/lifecycle.cpp
            src/debug/host_trace.cpp
        )
        target_compile_features(astrabot_adapter_entry_tests PRIVATE cxx_std_17)
        target_link_libraries(astrabot_adapter_entry_tests PRIVATE astrabot_core)
        target_include_directories(astrabot_adapter_entry_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
            SYSTEM PRIVATE
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

        add_executable(astrabot_fake_client_tests
            tests/adapter/fake_client_tests.cpp
            src/adapter/metamod/fake_client.cpp
            src/adapter/metamod/lifecycle.cpp
            src/adapter/metamod/plugin_entry.cpp
            src/debug/host_trace.cpp
        )
        target_compile_features(astrabot_fake_client_tests PRIVATE cxx_std_17)
        target_link_libraries(astrabot_fake_client_tests PRIVATE astrabot_core)
        target_include_directories(astrabot_fake_client_tests
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
            SYSTEM PRIVATE
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
    endif()
endif()
