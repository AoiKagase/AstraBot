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
  src/core/world/world_snapshot.cpp
  src/core/perception/perception.cpp
  src/core/behavior/objective_proposal.cpp
  src/core/behavior/behavior_state.cpp
  src/core/combat/weapon_state.cpp
  src/core/combat/combat_intent.cpp
  src/core/objectives/objective_state.cpp
  src/core/objectives/round_objectives.cpp
  src/core/team/team_report.cpp
  src/core/team/radio_intent.cpp
  src/core/runtime/actor_registry.cpp
  src/core/compat/command_registry.cpp
  src/core/compat/cvar_state.cpp
  src/core/compat/observation.cpp
  src/core/compat/runtime_mode_policy.cpp
  src/core/compat/random_source.cpp
  src/core/compat/bot_configuration.cpp
  src/core/compat/profile_catalog.cpp
  src/core/compat/state_machine.cpp
  src/core/nav/nav_model.cpp
  src/core/nav/legacy_nav_reader.cpp
  src/core/nav/nav_snapshot.cpp
  src/core/nav/nav_query.cpp
  src/core/nav/locomotion.cpp
  src/core/nav/jump_drop.cpp
  src/core/nav/special_traversal.cpp
	  src/core/runtime/bot_command.cpp
	src/core/runtime/nav_roam_controller.cpp
	src/core/runtime/movement_physics.cpp
  src/core/runtime/command_queue.cpp
  src/core/runtime/bot_timing_scheduler.cpp
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

  add_executable(astrabot_command_queue
    tests/command_queue_tests.cpp
  )
  target_include_directories(astrabot_command_queue PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_command_queue PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_command_queue)
  add_test(NAME astrabot_command_queue COMMAND astrabot_command_queue)

  add_executable(astrabot_join_controller
    tests/join_controller_tests.cpp
    src/adapter/metamod/join_controller.cpp
  )
  target_include_directories(astrabot_join_controller PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${ASTRABOT_METAMOD_SDK_ROOT}/metamod"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/common"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/dlls"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/engine"
    "${ASTRABOT_METAMOD_SDK_ROOT}/hlsdk/pm_shared"
  )
  astrabot_apply_warnings(astrabot_join_controller)
  add_test(NAME astrabot_join_controller COMMAND astrabot_join_controller)

  add_executable(astrabot_nav_roam_controller
    tests/nav_roam_controller_tests.cpp
  )
  target_include_directories(astrabot_nav_roam_controller PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_nav_roam_controller PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_nav_roam_controller)
  add_test(NAME astrabot_nav_roam_controller COMMAND astrabot_nav_roam_controller)

  add_executable(astrabot_nav_legacy_limit
    tests/nav_legacy_limit_tests.cpp
  )
  target_include_directories(astrabot_nav_legacy_limit PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_nav_legacy_limit PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_nav_legacy_limit)
  add_test(NAME astrabot_nav_legacy_limit COMMAND astrabot_nav_legacy_limit)

  add_executable(astrabot_compat_command_registry
    tests/compat_command_registry_tests.cpp
  )
  target_include_directories(astrabot_compat_command_registry PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_compat_command_registry PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_compat_command_registry)
  add_test(NAME astrabot_compat_command_registry COMMAND astrabot_compat_command_registry)

  add_executable(astrabot_compat_cvar_state
    tests/compat_cvar_state_tests.cpp
  )
  target_include_directories(astrabot_compat_cvar_state PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_compat_cvar_state PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_compat_cvar_state)
  add_test(NAME astrabot_compat_cvar_state COMMAND astrabot_compat_cvar_state)

  add_executable(astrabot_observation_model
    tests/observation_model_tests.cpp
  )
  target_include_directories(astrabot_observation_model PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_observation_model PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_observation_model)
  add_test(NAME astrabot_observation_model COMMAND astrabot_observation_model)

  add_executable(astrabot_compat_state_machine
    tests/compat_state_machine_tests.cpp
  )
  target_include_directories(astrabot_compat_state_machine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_compat_state_machine PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_compat_state_machine)
  add_test(NAME astrabot_compat_state_machine COMMAND astrabot_compat_state_machine)

  add_executable(astrabot_profile_catalog
    tests/profile_catalog_tests.cpp
  )
  target_include_directories(astrabot_profile_catalog PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_profile_catalog PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_profile_catalog)
  add_test(NAME astrabot_profile_catalog COMMAND astrabot_profile_catalog)

  add_executable(astrabot_profile_loader
    tests/profile_loader_tests.cpp
    src/adapter/metamod/profile_loader.cpp
  )
  target_include_directories(astrabot_profile_loader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
  )
  target_link_libraries(astrabot_profile_loader PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_profile_loader)
  add_test(NAME astrabot_profile_loader COMMAND astrabot_profile_loader)

  add_executable(astrabot_bot_configuration
    tests/bot_configuration_tests.cpp
  )
  target_include_directories(astrabot_bot_configuration PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_bot_configuration PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_bot_configuration)
  add_test(NAME astrabot_bot_configuration COMMAND astrabot_bot_configuration)

  add_executable(astrabot_runtime_mode_policy
    tests/runtime_mode_policy_tests.cpp
  )
  target_include_directories(astrabot_runtime_mode_policy PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_runtime_mode_policy PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_runtime_mode_policy)
  add_test(NAME astrabot_runtime_mode_policy COMMAND astrabot_runtime_mode_policy)

  add_executable(astrabot_runtime_timing
    tests/runtime_timing_tests.cpp
  )
  target_include_directories(astrabot_runtime_timing PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_runtime_timing PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_runtime_timing)
  add_test(NAME astrabot_runtime_timing COMMAND astrabot_runtime_timing)

  add_executable(astrabot_compatibility_rng
    tests/compatibility_rng_tests.cpp
  )
  target_include_directories(astrabot_compatibility_rng PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_compatibility_rng PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_compatibility_rng)
  add_test(NAME astrabot_compatibility_rng COMMAND astrabot_compatibility_rng)

  add_executable(astrabot_engine_random_source
    tests/engine_random_source_tests.cpp
    src/adapter/metamod/engine_random_source.cpp
  )
  target_include_directories(astrabot_engine_random_source PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
  )
  target_link_libraries(astrabot_engine_random_source PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_engine_random_source)
  add_test(NAME astrabot_engine_random_source COMMAND astrabot_engine_random_source)

  add_executable(astrabot_nav_model
    tests/nav_model_tests.cpp
  )
  target_include_directories(astrabot_nav_model PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_nav_model PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_nav_model)
  add_test(NAME astrabot_nav_model COMMAND astrabot_nav_model)

  add_executable(astrabot_legacy_nav_reader
    tests/legacy_nav_reader_tests.cpp
  )
  target_include_directories(astrabot_legacy_nav_reader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(astrabot_legacy_nav_reader PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_legacy_nav_reader)
  add_test(NAME astrabot_legacy_nav_reader COMMAND astrabot_legacy_nav_reader)

  add_executable(astrabot_nav_loader
    tests/nav_loader_tests.cpp
    src/adapter/metamod/nav_loader.cpp
    src/adapter/metamod/map_fingerprint.cpp
  )
  target_link_libraries(astrabot_nav_loader PRIVATE astrabot_core)
  target_include_directories(astrabot_nav_loader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_nav_loader)
  add_test(NAME astrabot_nav_loader COMMAND astrabot_nav_loader)

  add_executable(astrabot_nav_diagnostics
    tests/nav_diagnostics_tests.cpp
    src/adapter/metamod/nav_loader.cpp
    src/adapter/metamod/map_fingerprint.cpp
  )
  target_link_libraries(astrabot_nav_diagnostics PRIVATE astrabot_core)
  target_include_directories(astrabot_nav_diagnostics PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_nav_diagnostics)
  add_test(NAME astrabot_nav_diagnostics COMMAND astrabot_nav_diagnostics)

  add_executable(astrabot_nav_snapshot
    tests/nav_snapshot_tests.cpp
  )
  target_link_libraries(astrabot_nav_snapshot PRIVATE astrabot_core)
  target_include_directories(astrabot_nav_snapshot PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_nav_snapshot)
  add_test(NAME astrabot_nav_snapshot COMMAND astrabot_nav_snapshot)

  add_executable(astrabot_nav_query
    tests/nav_query_tests.cpp
  )
  target_link_libraries(astrabot_nav_query PRIVATE astrabot_core)
  target_include_directories(astrabot_nav_query PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_nav_query)
  add_test(NAME astrabot_nav_query COMMAND astrabot_nav_query)

  add_executable(astrabot_locomotion
    tests/locomotion_tests.cpp
  )
  target_link_libraries(astrabot_locomotion PRIVATE astrabot_core)
  target_include_directories(astrabot_locomotion PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_locomotion)
  add_test(NAME astrabot_locomotion COMMAND astrabot_locomotion)

  add_executable(astrabot_jump_drop
    tests/jump_drop_tests.cpp
  )
  target_link_libraries(astrabot_jump_drop PRIVATE astrabot_core)
  target_include_directories(astrabot_jump_drop PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_jump_drop)
  add_test(NAME astrabot_jump_drop COMMAND astrabot_jump_drop)

  add_executable(astrabot_special_traversal
    tests/special_traversal_tests.cpp
  )
  target_link_libraries(astrabot_special_traversal PRIVATE astrabot_core)
  target_include_directories(astrabot_special_traversal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_special_traversal)
  add_test(NAME astrabot_special_traversal COMMAND astrabot_special_traversal)

  add_executable(astrabot_world_snapshot
    tests/world_snapshot_tests.cpp
  )
  target_link_libraries(astrabot_world_snapshot PRIVATE astrabot_core)
  target_include_directories(astrabot_world_snapshot PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_world_snapshot)
  add_test(NAME astrabot_world_snapshot COMMAND astrabot_world_snapshot)

  add_executable(astrabot_perception
    tests/perception_tests.cpp
  )
  target_link_libraries(astrabot_perception PRIVATE astrabot_core)
  target_include_directories(astrabot_perception PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_perception)
  add_test(NAME astrabot_perception COMMAND astrabot_perception)

  add_executable(astrabot_behavior_state
    tests/behavior_state_tests.cpp
  )
  target_link_libraries(astrabot_behavior_state PRIVATE astrabot_core)
  target_include_directories(astrabot_behavior_state PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_behavior_state)
  add_test(NAME astrabot_behavior_state COMMAND astrabot_behavior_state)

  add_executable(astrabot_objective_proposal
    tests/objective_proposal_tests.cpp
  )
  target_link_libraries(astrabot_objective_proposal PRIVATE astrabot_core)
  target_include_directories(astrabot_objective_proposal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_objective_proposal)
  add_test(NAME astrabot_objective_proposal COMMAND astrabot_objective_proposal)

  add_executable(astrabot_weapon_state
    tests/weapon_state_tests.cpp
  )
  target_link_libraries(astrabot_weapon_state PRIVATE astrabot_core)
  target_include_directories(astrabot_weapon_state PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_weapon_state)
  add_test(NAME astrabot_weapon_state COMMAND astrabot_weapon_state)

  add_executable(astrabot_combat_intent
    tests/combat_intent_tests.cpp
  )
  target_link_libraries(astrabot_combat_intent PRIVATE astrabot_core)
  target_include_directories(astrabot_combat_intent PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_combat_intent)
  add_test(NAME astrabot_combat_intent COMMAND astrabot_combat_intent)

  add_executable(astrabot_objective_state
    tests/objective_state_tests.cpp
  )
  target_link_libraries(astrabot_objective_state PRIVATE astrabot_core)
  target_include_directories(astrabot_objective_state PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_objective_state)
  add_test(NAME astrabot_objective_state COMMAND astrabot_objective_state)

  add_executable(astrabot_round_objectives
    tests/round_objectives_tests.cpp
  )
  target_link_libraries(astrabot_round_objectives PRIVATE astrabot_core)
  target_include_directories(astrabot_round_objectives PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_round_objectives)
  add_test(NAME astrabot_round_objectives COMMAND astrabot_round_objectives)

  add_executable(astrabot_team_report
    tests/team_report_tests.cpp
  )
  target_link_libraries(astrabot_team_report PRIVATE astrabot_core)
  target_include_directories(astrabot_team_report PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_team_report)
  add_test(NAME astrabot_team_report COMMAND astrabot_team_report)

  add_executable(astrabot_radio_intent
    tests/radio_intent_tests.cpp
  )
  target_link_libraries(astrabot_radio_intent PRIVATE astrabot_core)
  target_include_directories(astrabot_radio_intent PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_radio_intent)
  add_test(NAME astrabot_radio_intent COMMAND astrabot_radio_intent)

  add_executable(astrabot_map_fingerprint
    tests/map_fingerprint_tests.cpp
    src/adapter/metamod/map_fingerprint.cpp
  )
  target_include_directories(astrabot_map_fingerprint PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_map_fingerprint)
	add_test(NAME astrabot_map_fingerprint COMMAND astrabot_map_fingerprint)

	add_executable(astrabot_movement_physics
		tests/movement_physics_tests.cpp
	)
	target_link_libraries(astrabot_movement_physics PRIVATE astrabot_core)
	astrabot_apply_warnings(astrabot_movement_physics)
	add_test(NAME astrabot_movement_physics COMMAND astrabot_movement_physics)
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
  set(ASTRABOT_CSSDK_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/dlls"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/engine"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/game_shared"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/pm_shared"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/public"
  )

  add_library(astrabot_mm SHARED
    src/adapter/metamod/compat_surface.cpp
    src/adapter/metamod/profile_loader.cpp
    src/adapter/metamod/fake_client_manager.cpp
    src/adapter/metamod/join_controller.cpp
    src/adapter/metamod/input_dispatcher.cpp
    src/adapter/metamod/plugin_exports.cpp
    src/adapter/metamod/native_bot_guard.cpp
	src/adapter/metamod/plugin_runtime.cpp
	src/adapter/metamod/engine_random_source.cpp
	src/adapter/metamod/map_fingerprint.cpp
	src/adapter/metamod/nav_loader.cpp
	src/adapter/metamod/action_adapter.cpp
	src/adapter/metamod/observation_adapter.cpp
	)
  target_include_directories(astrabot_mm PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    ${ASTRABOT_CSSDK_INCLUDE_DIRS}
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
    target_link_options(astrabot_mm PRIVATE
      "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod/astrabot_mm.map"
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
      src/adapter/metamod/compat_surface.cpp
      src/adapter/metamod/profile_loader.cpp
      src/adapter/metamod/fake_client_manager.cpp
      src/adapter/metamod/input_dispatcher.cpp
      src/adapter/metamod/native_bot_guard.cpp
      src/adapter/metamod/plugin_exports.cpp
      src/adapter/metamod/join_controller.cpp
		src/adapter/metamod/plugin_runtime.cpp
		src/adapter/metamod/engine_random_source.cpp
		src/adapter/metamod/nav_loader.cpp
		src/adapter/metamod/map_fingerprint.cpp
		src/adapter/metamod/action_adapter.cpp
		src/adapter/metamod/observation_adapter.cpp
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

    add_executable(astrabot_input_dispatcher
      tests/input_dispatcher_tests.cpp
      src/adapter/metamod/input_dispatcher.cpp
    )
    target_include_directories(astrabot_input_dispatcher PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_input_dispatcher PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_input_dispatcher)
    add_test(NAME astrabot_input_dispatcher COMMAND astrabot_input_dispatcher)

  add_executable(astrabot_fake_client_isolation
    tests/fake_client_isolation_tests.cpp
    src/adapter/metamod/compat_surface.cpp
    src/adapter/metamod/profile_loader.cpp
    src/adapter/metamod/fake_client_manager.cpp
      src/adapter/metamod/input_dispatcher.cpp
      src/adapter/metamod/native_bot_guard.cpp
      src/adapter/metamod/plugin_exports.cpp
      src/adapter/metamod/join_controller.cpp
		src/adapter/metamod/plugin_runtime.cpp
		src/adapter/metamod/engine_random_source.cpp
		src/adapter/metamod/nav_loader.cpp
		src/adapter/metamod/map_fingerprint.cpp
		src/adapter/metamod/action_adapter.cpp
		src/adapter/metamod/observation_adapter.cpp
	)
    target_include_directories(astrabot_fake_client_isolation PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_fake_client_isolation PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_fake_client_isolation)
    add_test(NAME astrabot_fake_client_isolation COMMAND astrabot_fake_client_isolation)

    add_executable(astrabot_compat_surface
      tests/compat_surface_tests.cpp
      src/adapter/metamod/compat_surface.cpp
      src/adapter/metamod/profile_loader.cpp
    )
    target_include_directories(astrabot_compat_surface PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_compat_surface PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_compat_surface)
    add_test(NAME astrabot_compat_surface COMMAND astrabot_compat_surface)

add_executable(astrabot_compat_actor_command
	tests/compat_actor_command_tests.cpp
      src/adapter/metamod/compat_surface.cpp
      src/adapter/metamod/profile_loader.cpp
      src/adapter/metamod/fake_client_manager.cpp
      src/adapter/metamod/input_dispatcher.cpp
      src/adapter/metamod/native_bot_guard.cpp
      src/adapter/metamod/plugin_exports.cpp
      src/adapter/metamod/join_controller.cpp
      src/adapter/metamod/plugin_runtime.cpp
      src/adapter/metamod/engine_random_source.cpp
	src/adapter/metamod/nav_loader.cpp
	src/adapter/metamod/map_fingerprint.cpp
	src/adapter/metamod/action_adapter.cpp
	src/adapter/metamod/observation_adapter.cpp
)
    target_include_directories(astrabot_compat_actor_command PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_compat_actor_command PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_compat_actor_command)
	add_test(NAME astrabot_compat_actor_command COMMAND astrabot_compat_actor_command)

	add_executable(astrabot_observation_adapter
    tests/observation_adapter_tests.cpp
    src/adapter/metamod/observation_adapter.cpp
  )
	target_include_directories(astrabot_observation_adapter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
    ${ASTRABOT_METAMOD_INCLUDE_DIRS}
  )
	target_link_libraries(astrabot_observation_adapter PRIVATE astrabot_core)
	astrabot_apply_warnings(astrabot_observation_adapter)
	add_test(NAME astrabot_observation_adapter COMMAND astrabot_observation_adapter)

	add_executable(astrabot_perception_adapter
		tests/perception_adapter_tests.cpp
		src/adapter/metamod/observation_adapter.cpp
	)
	target_include_directories(astrabot_perception_adapter PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
		${ASTRABOT_METAMOD_INCLUDE_DIRS}
	)
	target_link_libraries(astrabot_perception_adapter PRIVATE astrabot_core)
	astrabot_apply_warnings(astrabot_perception_adapter)
	add_test(NAME astrabot_perception_adapter COMMAND astrabot_perception_adapter)

	add_executable(astrabot_observation_trace
		tests/observation_trace_tests.cpp
		src/adapter/metamod/observation_adapter.cpp
	)
	target_include_directories(astrabot_observation_trace PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
		${ASTRABOT_METAMOD_INCLUDE_DIRS}
	)
	target_link_libraries(astrabot_observation_trace PRIVATE astrabot_core)
	astrabot_apply_warnings(astrabot_observation_trace)
	add_test(NAME astrabot_observation_trace COMMAND astrabot_observation_trace)

	add_executable(astrabot_action_adapter
		tests/action_adapter_tests.cpp
		src/adapter/metamod/action_adapter.cpp
	)
	target_include_directories(astrabot_action_adapter PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
		${ASTRABOT_METAMOD_INCLUDE_DIRS}
	)
	astrabot_apply_warnings(astrabot_action_adapter)
	add_test(NAME astrabot_action_adapter COMMAND astrabot_action_adapter)
endif()
endif()

if(ASTRABOT_BUILD_TESTS)
  get_property(ASTRABOT_REGISTERED_TESTS DIRECTORY PROPERTY TESTS)
  set_tests_properties(
    ${ASTRABOT_REGISTERED_TESTS}
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()
