cmake_minimum_required(VERSION 3.20)

project(AstraBot VERSION 0.1.0 LANGUAGES C CXX)

option(ASTRABOT_BUILD_METAMOD "Build the Metamod-P plugin target" OFF)
option(ASTRABOT_BUILD_TESTS "Build portable and adapter tests" OFF)
option(ASTRABOT_WARNINGS_AS_ERRORS "Treat AstraBot warnings as errors" ON)
set(ASTRABOT_METAMOD_SDK_ROOT "" CACHE PATH "Path to the pinned Metamod-P checkout")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "AstraBot requires a 32-bit x86 target; configure with an x86 toolchain")
endif()

include(cmake/AstraBotBuild.cmake)

add_library(astrabot_core STATIC
  src/core/build_identity.cpp
  src/core/runtime/actor_registry.cpp
  src/core/runtime/lifecycle.cpp
)
target_include_directories(astrabot_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(NOT MSVC)
  set_target_properties(astrabot_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
endif()
target_compile_definitions(astrabot_core PRIVATE
  ASTRABOT_CXX_STANDARD=14
  ASTRABOT_VERSION_STRING="${PROJECT_VERSION}"
)
astrabot_apply_warnings(astrabot_core)

if(ASTRABOT_BUILD_TESTS)
  enable_testing()

  add_executable(astrabot_build_identity
    tests/build_identity_tests.cpp
  )
  target_include_directories(astrabot_build_identity PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_build_identity PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_build_identity)
  add_test(NAME astrabot_build_identity COMMAND astrabot_build_identity)

  add_executable(astrabot_lifecycle_generation
    tests/lifecycle_generation_tests.cpp
  )
  target_include_directories(astrabot_lifecycle_generation PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_lifecycle_generation PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_lifecycle_generation)
  add_test(NAME astrabot_lifecycle_generation COMMAND astrabot_lifecycle_generation)

  add_executable(astrabot_native_bot_guard
    tests/native_bot_guard_tests.cpp
    src/adapter/metamod/native_bot_guard.cpp
  )
  target_include_directories(astrabot_native_bot_guard PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_native_bot_guard)
  add_test(NAME astrabot_native_bot_guard COMMAND astrabot_native_bot_guard)

  add_executable(astrabot_actor_registry
    tests/actor_registry_tests.cpp
  )
  target_include_directories(astrabot_actor_registry PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_actor_registry PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_actor_registry)
  add_test(NAME astrabot_actor_registry COMMAND astrabot_actor_registry)
endif()

if(ASTRABOT_BUILD_METAMOD)
  if(NOT IS_DIRECTORY "${ASTRABOT_METAMOD_SDK_ROOT}/metamod" OR
     NOT IS_DIRECTORY "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk")
    message(FATAL_ERROR
      "ASTRABOT_METAMOD_SDK_ROOT must contain the pinned metamod and hlsdk directories")
  endif()

  set(ASTRABOT_METAMOD_INCLUDE_DIRS
    "${ASTRABOT_METAMOD_SDK_ROOT}/metamod"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/common"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/dlls"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/engine"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/pm_shared"
  )

  add_library(astrabot_mm SHARED
    src/adapter/metamod/fake_client_manager.cpp
    src/adapter/metamod/plugin_exports.cpp
    src/adapter/metamod/native_bot_guard.cpp
    src/adapter/metamod/plugin_runtime.cpp
  )
  target_include_directories(astrabot_mm PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${ASTRABOT_METAMOD_INCLUDE_DIRS}
  )
  target_compile_definitions(astrabot_mm PRIVATE
    ASTRABOT_CXX_STANDARD=14
    ASTRABOT_VERSION_STRING="${PROJECT_VERSION}"
  )
  target_link_libraries(astrabot_mm PRIVATE astrabot_core)
  if(NOT MSVC)
    set_target_properties(astrabot_mm PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN ON
    )
  endif()
  if(MSVC)
    target_sources(astrabot_mm PRIVATE
      src/adapter/metamod/astrabot_mm.def
    )
  endif()
  astrabot_apply_warnings(astrabot_mm)

  if(ASTRABOT_BUILD_TESTS)
    add_executable(astrabot_metamod_abi
      tests/metamod_abi_tests.cpp
    )
    target_include_directories(astrabot_metamod_abi PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_metamod_abi PRIVATE astrabot_mm)
    astrabot_apply_warnings(astrabot_metamod_abi)
    add_test(NAME astrabot_metamod_abi COMMAND astrabot_metamod_abi)

    add_executable(astrabot_metamod_hooks
      tests/metamod_hook_table_tests.cpp
    )
    target_include_directories(astrabot_metamod_hooks PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_sources(astrabot_metamod_hooks PRIVATE
      src/adapter/metamod/fake_client_manager.cpp
      src/adapter/metamod/native_bot_guard.cpp
      src/adapter/metamod/plugin_exports.cpp
      src/adapter/metamod/plugin_runtime.cpp
    )
    target_link_libraries(astrabot_metamod_hooks PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_metamod_hooks)
    add_test(NAME astrabot_metamod_hooks COMMAND astrabot_metamod_hooks)

    add_executable(astrabot_fake_client_manager
      tests/fake_client_manager_tests.cpp
      src/adapter/metamod/fake_client_manager.cpp
    )
    target_include_directories(astrabot_fake_client_manager PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_fake_client_manager PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_fake_client_manager)
    add_test(NAME astrabot_fake_client_manager COMMAND astrabot_fake_client_manager)
  endif()
endif()
