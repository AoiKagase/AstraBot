cmake_minimum_required(VERSION 3.20)

project(AstraBot VERSION 0.1.0 LANGUAGES C CXX)

option(ASTRABOT_BUILD_METAMOD "Build the Metamod-P plugin target" OFF)
option(ASTRABOT_BUILD_TESTS "Build portable and adapter tests" OFF)
option(ASTRABOT_WARNINGS_AS_ERRORS "Treat AstraBot warnings as errors" ON)
set(ASTRABOT_METAMOD_SDK_ROOT "" CACHE PATH "Path to the pinned Metamod-P checkout")
set(ASTRABOT_DE_DUST2_NAV "" CACHE FILEPATH
	"External de_dust2.nav path for the NAV performance regression test")

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
  src/core/compat/noise_navigation.cpp
  src/core/behavior/objective_proposal.cpp
  src/core/behavior/behavior_state.cpp
  src/core/combat/weapon_state.cpp
  src/core/combat/combat_intent.cpp
  src/core/objectives/objective_state.cpp
  src/core/objectives/round_objectives.cpp
  src/core/objectives/plant_attempt.cpp
  src/core/team/team_report.cpp
  src/core/team/round_objective_coordinator.cpp
  src/core/team/radio_intent.cpp
  src/core/runtime/actor_registry.cpp
  src/core/compat/command_registry.cpp
  src/core/compat/cvar_state.cpp
  src/core/compat/observation.cpp
  src/core/compat/runtime_mode_policy.cpp
  src/core/compat/random_source.cpp
  src/core/compat/bot_configuration.cpp
  src/core/compat/buy_controller.cpp
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
  src/core/runtime/jump_crouch_sequencer.cpp
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

  add_executable(astrabot_plant_attempt tests/plant_attempt_tests.cpp)
  target_link_libraries(astrabot_plant_attempt PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_plant_attempt)
  add_test(NAME astrabot_plant_attempt COMMAND astrabot_plant_attempt)

  add_executable(astrabot_public_observation tests/public_observation_tests.cpp)
  target_link_libraries(astrabot_public_observation PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_public_observation)
  add_test(NAME astrabot_public_observation COMMAND astrabot_public_observation)

  add_executable(astrabot_vanilla_management tests/vanilla_management_tests.cpp)
  target_link_libraries(astrabot_vanilla_management PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_vanilla_management)
  add_test(NAME astrabot_vanilla_management COMMAND astrabot_vanilla_management)

  add_executable(astrabot_vanilla_behavior tests/vanilla_behavior_tests.cpp)
  target_link_libraries(astrabot_vanilla_behavior PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_vanilla_behavior)
  add_test(NAME astrabot_vanilla_behavior COMMAND astrabot_vanilla_behavior)

  add_executable(astrabot_combat_policy tests/combat_policy_tests.cpp)
  add_executable(astrabot_weapon_equipment tests/weapon_equipment_tests.cpp)
  target_link_libraries(astrabot_weapon_equipment PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_weapon_equipment)
  add_test(NAME astrabot_weapon_equipment COMMAND astrabot_weapon_equipment)
  target_link_libraries(astrabot_combat_policy PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_combat_policy)
  add_test(NAME astrabot_combat_policy COMMAND astrabot_combat_policy)

  add_executable(astrabot_literal_precision tests/literal_precision_tests.cpp)
  target_link_libraries(astrabot_literal_precision PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_literal_precision)
  add_test(NAME astrabot_literal_precision COMMAND astrabot_literal_precision)

  add_executable(astrabot_weapon_actions tests/weapon_actions_tests.cpp)
  target_link_libraries(astrabot_weapon_actions PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_weapon_actions)
  add_test(NAME astrabot_weapon_actions COMMAND astrabot_weapon_actions)

  add_executable(astrabot_scenario_behavior tests/scenario_behavior_tests.cpp)
  target_link_libraries(astrabot_scenario_behavior PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_scenario_behavior)
  add_test(NAME astrabot_scenario_behavior COMMAND astrabot_scenario_behavior)

  add_executable(astrabot_public_bomb_clock tests/public_bomb_clock_tests.cpp)
  target_link_libraries(astrabot_public_bomb_clock PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_public_bomb_clock)
  add_test(NAME astrabot_public_bomb_clock COMMAND astrabot_public_bomb_clock)

  add_executable(astrabot_bomb_navigation tests/bomb_navigation_tests.cpp)
  target_link_libraries(astrabot_bomb_navigation PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_bomb_navigation)
  add_test(NAME astrabot_bomb_navigation COMMAND astrabot_bomb_navigation)

  add_executable(astrabot_death_interrupt tests/death_interrupt_tests.cpp)
  target_link_libraries(astrabot_death_interrupt PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_death_interrupt)
  add_test(NAME astrabot_death_interrupt COMMAND astrabot_death_interrupt)

  add_executable(astrabot_friend_death_chatter tests/friend_death_chatter_tests.cpp)
target_link_libraries(astrabot_friend_death_chatter PRIVATE astrabot_core)
astrabot_apply_warnings(astrabot_friend_death_chatter)
add_test(NAME astrabot_friend_death_chatter COMMAND astrabot_friend_death_chatter)

add_executable(astrabot_friend_death tests/friend_death_tests.cpp)
  target_link_libraries(astrabot_friend_death PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_friend_death)
  add_test(NAME astrabot_friend_death COMMAND astrabot_friend_death)
  add_executable(astrabot_knife_run tests/knife_run_tests.cpp)
  target_link_libraries(astrabot_knife_run PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_knife_run)
  add_test(NAME astrabot_knife_run COMMAND astrabot_knife_run)
  add_executable(astrabot_kill_praise tests/kill_praise_tests.cpp)
  target_link_libraries(astrabot_kill_praise PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_kill_praise)
  add_test(NAME astrabot_kill_praise COMMAND astrabot_kill_praise)

  add_executable(astrabot_chatter_schedule tests/chatter_schedule_tests.cpp)
target_link_libraries(astrabot_chatter_schedule PRIVATE astrabot_core)
astrabot_apply_warnings(astrabot_chatter_schedule)
add_test(NAME astrabot_chatter_schedule COMMAND astrabot_chatter_schedule)

add_executable(astrabot_chatter_playback tests/chatter_playback_tests.cpp)
target_link_libraries(astrabot_chatter_playback PRIVATE astrabot_core)
astrabot_apply_warnings(astrabot_chatter_playback)
add_test(NAME astrabot_chatter_playback COMMAND astrabot_chatter_playback)

add_executable(astrabot_chatter_statements tests/chatter_statements_tests.cpp)
target_link_libraries(astrabot_chatter_statements PRIVATE astrabot_core)
astrabot_apply_warnings(astrabot_chatter_statements)
add_test(NAME astrabot_chatter_statements COMMAND astrabot_chatter_statements)

add_executable(astrabot_enemy_count_chatter tests/enemy_count_chatter_tests.cpp)
  target_link_libraries(astrabot_enemy_count_chatter PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_enemy_count_chatter)
  add_test(NAME astrabot_enemy_count_chatter COMMAND astrabot_enemy_count_chatter)

  add_executable(astrabot_noise_report tests/noise_report_tests.cpp)
target_link_libraries(astrabot_noise_report PRIVATE astrabot_core)
astrabot_apply_warnings(astrabot_noise_report)
add_test(NAME astrabot_noise_report COMMAND astrabot_noise_report)
add_executable(astrabot_noise_investigation tests/noise_investigation_tests.cpp)
  target_link_libraries(astrabot_noise_investigation PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_noise_investigation)
  add_test(NAME astrabot_noise_investigation COMMAND astrabot_noise_investigation)
  add_executable(astrabot_noise_navigation tests/noise_navigation_tests.cpp)
  target_link_libraries(astrabot_noise_navigation PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_noise_navigation)
  add_test(NAME astrabot_noise_navigation COMMAND astrabot_noise_navigation)
  add_executable(astrabot_gunshot_events tests/gunshot_event_tests.cpp)
  target_link_libraries(astrabot_gunshot_events PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_gunshot_events)
  add_test(NAME astrabot_gunshot_events COMMAND astrabot_gunshot_events)
  add_executable(astrabot_noise_look tests/noise_look_tests.cpp)
  target_link_libraries(astrabot_noise_look PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_noise_look)
  add_test(NAME astrabot_noise_look COMMAND astrabot_noise_look)
  add_executable(astrabot_chatter_nav_projection tests/chatter_nav_projection_tests.cpp)
  target_link_libraries(astrabot_chatter_nav_projection PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_nav_projection)
  add_test(NAME astrabot_chatter_nav_projection COMMAND astrabot_chatter_nav_projection)
  add_executable(astrabot_chatter_enemy_place tests/chatter_enemy_place_tests.cpp)
  target_link_libraries(astrabot_chatter_enemy_place PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_enemy_place)
  add_test(NAME astrabot_chatter_enemy_place COMMAND astrabot_chatter_enemy_place)
  add_executable(astrabot_chatter_logical_speech tests/chatter_logical_speech_tests.cpp)
  target_link_libraries(astrabot_chatter_logical_speech PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_logical_speech)
  add_test(NAME astrabot_chatter_logical_speech COMMAND astrabot_chatter_logical_speech)
  add_executable(astrabot_chatter_statement_playback tests/chatter_statement_playback_tests.cpp)
  target_link_libraries(astrabot_chatter_statement_playback PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_statement_playback)
  add_test(NAME astrabot_chatter_statement_playback COMMAND astrabot_chatter_statement_playback)
  add_executable(astrabot_chatter_place_history tests/chatter_place_history_tests.cpp)
  target_link_libraries(astrabot_chatter_place_history PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_place_history)
  add_test(NAME astrabot_chatter_place_history COMMAND astrabot_chatter_place_history)

  add_executable(astrabot_chatter_places tests/chatter_places_tests.cpp)
  target_link_libraries(astrabot_chatter_places PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_places)
  add_test(NAME astrabot_chatter_places COMMAND astrabot_chatter_places)
  add_test(NAME astrabot_chatter_place_criteria COMMAND astrabot_chatter_places --criteria)
  add_test(NAME astrabot_chatter_place_parser COMMAND astrabot_chatter_places --parser)

  add_executable(astrabot_chatter_voice tests/chatter_voice_tests.cpp)
  target_link_libraries(astrabot_chatter_voice PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_voice)
  add_test(NAME astrabot_chatter_voice COMMAND astrabot_chatter_voice)
  add_executable(astrabot_outgoing_radio tests/outgoing_radio_tests.cpp)
  target_link_libraries(astrabot_outgoing_radio PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_outgoing_radio)
  add_test(NAME astrabot_outgoing_radio COMMAND astrabot_outgoing_radio)

  add_executable(astrabot_teamwork_policy tests/teamwork_policy_tests.cpp)
  target_link_libraries(astrabot_teamwork_policy PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_teamwork_policy)
  add_test(NAME astrabot_teamwork_policy COMMAND astrabot_teamwork_policy)

  add_executable(astrabot_help_request tests/help_request_tests.cpp)
  target_link_libraries(astrabot_help_request PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_help_request)
  add_test(NAME astrabot_help_request COMMAND astrabot_help_request)
  add_executable(astrabot_follow_request tests/follow_request_tests.cpp)
  target_link_libraries(astrabot_follow_request PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_follow_request)
  add_test(NAME astrabot_follow_request COMMAND astrabot_follow_request)

  add_executable(astrabot_defend_request tests/defend_request_tests.cpp)
  target_link_libraries(astrabot_defend_request PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_defend_request)
  add_test(NAME astrabot_defend_request COMMAND astrabot_defend_request)

  add_executable(astrabot_native_ladder tests/native_ladder_tests.cpp)
  target_link_libraries(astrabot_native_ladder PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_native_ladder)
  add_test(NAME astrabot_native_ladder COMMAND astrabot_native_ladder)

  add_executable(astrabot_nav_danger_memory tests/nav_danger_memory_tests.cpp)
  target_link_libraries(astrabot_nav_danger_memory PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_nav_danger_memory)
  add_test(NAME astrabot_nav_danger_memory COMMAND astrabot_nav_danger_memory)

  add_executable(astrabot_grenade_trajectory tests/grenade_trajectory_tests.cpp)
  target_link_libraries(astrabot_grenade_trajectory PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_grenade_trajectory)
  add_test(NAME astrabot_grenade_trajectory COMMAND astrabot_grenade_trajectory)

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
    src/adapter/metamod/lifecycle/native_bot_guard.cpp
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
    src/adapter/metamod/lifecycle/join_controller.cpp
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

  add_executable(astrabot_player_avoidance tests/player_avoidance_tests.cpp)
  target_include_directories(astrabot_player_avoidance PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(astrabot_player_avoidance PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_player_avoidance)
  add_test(NAME astrabot_player_avoidance COMMAND astrabot_player_avoidance)

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
    src/adapter/metamod/config/profile_loader.cpp
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
    src/adapter/metamod/bridge/engine_random_source.cpp
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
    src/adapter/metamod/navigation/nav_loader.cpp
    src/adapter/metamod/navigation/map_fingerprint.cpp
  )
  target_link_libraries(astrabot_nav_loader PRIVATE astrabot_core)
  target_include_directories(astrabot_nav_loader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_nav_loader)
add_test(NAME astrabot_nav_loader COMMAND astrabot_nav_loader)

add_executable(astrabot_nav_external_performance
	tests/nav_external_performance_tests.cpp
	src/adapter/metamod/navigation/nav_loader.cpp
	src/adapter/metamod/navigation/map_fingerprint.cpp
)
target_link_libraries(astrabot_nav_external_performance PRIVATE astrabot_core)
target_include_directories(astrabot_nav_external_performance PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)
astrabot_apply_warnings(astrabot_nav_external_performance)
add_test(NAME astrabot_nav_external_performance COMMAND astrabot_nav_external_performance)
set_tests_properties(astrabot_nav_external_performance PROPERTIES SKIP_RETURN_CODE 77)
if(ASTRABOT_DE_DUST2_NAV)
	set_tests_properties(astrabot_nav_external_performance PROPERTIES
		ENVIRONMENT "ASTRABOT_DE_DUST2_NAV=${ASTRABOT_DE_DUST2_NAV}")
endif()

add_executable(astrabot_nav_diagnostics
    tests/nav_diagnostics_tests.cpp
    src/adapter/metamod/navigation/nav_loader.cpp
    src/adapter/metamod/navigation/map_fingerprint.cpp
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

  add_executable(astrabot_round_objective_coordinator
    tests/round_objective_coordinator_tests.cpp
  )
  target_link_libraries(astrabot_round_objective_coordinator PRIVATE astrabot_core)
  target_include_directories(astrabot_round_objective_coordinator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  astrabot_apply_warnings(astrabot_round_objective_coordinator)
  add_test(NAME astrabot_round_objective_coordinator COMMAND astrabot_round_objective_coordinator)

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
    src/adapter/metamod/navigation/map_fingerprint.cpp
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

	add_executable(astrabot_jump_crouch_sequencer
		tests/jump_crouch_sequencer_tests.cpp
	)
	target_link_libraries(astrabot_jump_crouch_sequencer PRIVATE astrabot_core)
	astrabot_apply_warnings(astrabot_jump_crouch_sequencer)
	add_test(NAME astrabot_jump_crouch_sequencer COMMAND astrabot_jump_crouch_sequencer)
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
    src/adapter/metamod/config/compat_surface.cpp
    src/adapter/metamod/config/profile_loader.cpp
    src/adapter/metamod/lifecycle/fake_client_manager.cpp
    src/adapter/metamod/lifecycle/join_controller.cpp
    src/adapter/metamod/bridge/input_dispatcher.cpp
    src/adapter/metamod/plugin/plugin_exports.cpp
    src/adapter/metamod/lifecycle/native_bot_guard.cpp
	src/adapter/metamod/plugin/plugin_runtime.cpp
	src/adapter/metamod/plugin/plugin_movement_sound.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
	src/adapter/metamod/navigation/movement_execution_gate.cpp
	src/adapter/metamod/bridge/engine_random_source.cpp
	src/adapter/metamod/navigation/map_fingerprint.cpp
	src/adapter/metamod/navigation/nav_loader.cpp
	src/adapter/metamod/bridge/action_adapter.cpp
	src/adapter/metamod/bridge/observation_adapter.cpp
	)
  target_include_directories(astrabot_mm PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
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
      "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod/plugin/astrabot_mm.map"
    )
  endif()
  if(MSVC)
    target_sources(astrabot_mm PRIVATE
      src/adapter/metamod/plugin/astrabot_mm.def
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
      src/adapter/metamod/config/compat_surface.cpp
      src/adapter/metamod/config/profile_loader.cpp
      src/adapter/metamod/lifecycle/fake_client_manager.cpp
      src/adapter/metamod/bridge/input_dispatcher.cpp
      src/adapter/metamod/lifecycle/native_bot_guard.cpp
      src/adapter/metamod/plugin/plugin_exports.cpp
      src/adapter/metamod/lifecycle/join_controller.cpp
	src/adapter/metamod/plugin/plugin_runtime.cpp
	src/adapter/metamod/plugin/plugin_movement_sound.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
	src/adapter/metamod/navigation/movement_execution_gate.cpp
		src/adapter/metamod/bridge/engine_random_source.cpp
		src/adapter/metamod/navigation/nav_loader.cpp
		src/adapter/metamod/navigation/map_fingerprint.cpp
		src/adapter/metamod/bridge/action_adapter.cpp
		src/adapter/metamod/bridge/observation_adapter.cpp
	)
    target_link_libraries(astrabot_metamod_hooks PRIVATE astrabot_core)
    astrabot_apply_warnings(astrabot_metamod_hooks)
    add_test(NAME astrabot_metamod_hooks COMMAND astrabot_metamod_hooks)

    add_executable(astrabot_fake_client_manager
      tests/fake_client_manager_tests.cpp
      src/adapter/metamod/lifecycle/fake_client_manager.cpp
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
      src/adapter/metamod/bridge/input_dispatcher.cpp
      src/adapter/metamod/diagnostics/runtime_profiler.cpp
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
    src/adapter/metamod/config/compat_surface.cpp
    src/adapter/metamod/config/profile_loader.cpp
    src/adapter/metamod/lifecycle/fake_client_manager.cpp
      src/adapter/metamod/bridge/input_dispatcher.cpp
      src/adapter/metamod/lifecycle/native_bot_guard.cpp
      src/adapter/metamod/plugin/plugin_exports.cpp
      src/adapter/metamod/lifecycle/join_controller.cpp
	src/adapter/metamod/plugin/plugin_runtime.cpp
	src/adapter/metamod/plugin/plugin_movement_sound.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
	src/adapter/metamod/navigation/movement_execution_gate.cpp
		src/adapter/metamod/bridge/engine_random_source.cpp
		src/adapter/metamod/navigation/nav_loader.cpp
		src/adapter/metamod/navigation/map_fingerprint.cpp
		src/adapter/metamod/bridge/action_adapter.cpp
		src/adapter/metamod/bridge/observation_adapter.cpp
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
      src/adapter/metamod/config/compat_surface.cpp
      src/adapter/metamod/config/profile_loader.cpp
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
      src/adapter/metamod/config/compat_surface.cpp
      src/adapter/metamod/config/profile_loader.cpp
      src/adapter/metamod/lifecycle/fake_client_manager.cpp
      src/adapter/metamod/bridge/input_dispatcher.cpp
      src/adapter/metamod/lifecycle/native_bot_guard.cpp
      src/adapter/metamod/plugin/plugin_exports.cpp
      src/adapter/metamod/lifecycle/join_controller.cpp
	src/adapter/metamod/plugin/plugin_runtime.cpp
	src/adapter/metamod/plugin/plugin_movement_sound.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
	src/adapter/metamod/navigation/movement_execution_gate.cpp
      src/adapter/metamod/bridge/engine_random_source.cpp
	src/adapter/metamod/navigation/nav_loader.cpp
	src/adapter/metamod/navigation/map_fingerprint.cpp
	src/adapter/metamod/bridge/action_adapter.cpp
	src/adapter/metamod/bridge/observation_adapter.cpp
)
    target_include_directories(astrabot_compat_actor_command PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
      ${ASTRABOT_METAMOD_INCLUDE_DIRS}
    )
    target_link_libraries(astrabot_compat_actor_command PRIVATE astrabot_core)
    # Preserve the frozen full-diagnostic capture independently of normal logging.
    target_compile_definitions(astrabot_compat_actor_command PRIVATE ASTRABOT_TEST_FULL_DIAGNOSTICS)
    astrabot_apply_warnings(astrabot_compat_actor_command)
	add_test(NAME astrabot_compat_actor_command COMMAND astrabot_compat_actor_command)

add_executable(astrabot_observation_adapter
	tests/observation_adapter_tests.cpp
	src/adapter/metamod/bridge/observation_adapter.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
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
	src/adapter/metamod/bridge/observation_adapter.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
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
	src/adapter/metamod/bridge/observation_adapter.cpp
	src/adapter/metamod/diagnostics/runtime_profiler.cpp
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
		src/adapter/metamod/bridge/action_adapter.cpp
	)
	target_include_directories(astrabot_action_adapter PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod"
		${ASTRABOT_METAMOD_INCLUDE_DIRS}
	)
	astrabot_apply_warnings(astrabot_action_adapter)
	add_test(NAME astrabot_action_adapter COMMAND astrabot_action_adapter)
	add_executable(astrabot_movement_execution_gate
		tests/movement_execution_gate_tests.cpp
		src/adapter/metamod/navigation/movement_execution_gate.cpp
	)
	target_include_directories(astrabot_movement_execution_gate PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		${ASTRABOT_METAMOD_INCLUDE_DIRS}
	)
	astrabot_apply_warnings(astrabot_movement_execution_gate)
	add_test(NAME astrabot_movement_execution_gate COMMAND astrabot_movement_execution_gate)
	add_executable(astrabot_runtime_profiler
		tests/runtime_profiler_tests.cpp
		src/adapter/metamod/diagnostics/runtime_profiler.cpp
	)
	target_include_directories(astrabot_runtime_profiler PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
	)
	astrabot_apply_warnings(astrabot_runtime_profiler)
	add_test(NAME astrabot_runtime_profiler COMMAND astrabot_runtime_profiler)
endif()
endif()

if(ASTRABOT_BUILD_METAMOD)
  # The bundled public API header also includes incomplete GameDLL private
  # headers. Project its exact official interface declaration, not a handwritten
  # vtable. Configuration fails if that declaration changes shape.
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/third_party/regamedll-5.30/regamedll_api.h" regame_api_source)
  string(REPLACE "\r\n" "\n" regame_api_source "${regame_api_source}")
  string(FIND "${regame_api_source}" "class IReGameApi {" regame_api_begin)
  string(FIND "${regame_api_source}" "#pragma once" regame_license_end)
  if(regame_api_begin LESS 0 OR regame_license_end LESS 0)
    message(FATAL_ERROR "Official IReGameApi declaration or license preamble was not found")
  endif()
  string(SUBSTRING "${regame_api_source}" 0 ${regame_license_end} regame_license)
  string(SUBSTRING "${regame_api_source}" ${regame_api_begin} -1 regame_api_tail)
  string(FIND "${regame_api_tail}" "\n};" regame_api_end)
  if(regame_api_end LESS 0)
    message(FATAL_ERROR "Official IReGameApi declaration has no closing boundary")
  endif()
  math(EXPR regame_api_length "${regame_api_end} + 3")
  string(SUBSTRING "${regame_api_tail}" 0 ${regame_api_length} regame_api_declaration)
  set(regame_api_defines "")
  foreach(regame_define REGAMEDLL_API_VERSION_MAJOR REGAMEDLL_API_VERSION_MINOR VRE_GAMEDLL_API_VERSION)
    string(REGEX MATCH "#define ${regame_define} [^\n]+" regame_definition "${regame_api_source}")
    if(NOT regame_definition)
      message(FATAL_ERROR "Official ${regame_define} definition was not found")
    endif()
    string(APPEND regame_api_defines "${regame_definition}\n")
  endforeach()
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/dlls/weapontype.h" regame_weapon_types)
  set(regame_api_types "")
  foreach(regame_type WeaponIdType AmmoType)
    string(REGEX MATCH "enum ${regame_type}[^{}]*\\{[^{}]*\\};" regame_enum "${regame_weapon_types}")
    if(NOT regame_enum)
      message(FATAL_ERROR "Official ${regame_type} enum was not found")
    endif()
    string(APPEND regame_api_types "${regame_enum}\n")
  endforeach()
  foreach(regame_enum_source player.h items.h gamerules.h extdef.h)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/dlls/${regame_enum_source}" regame_enums)
    foreach(regame_type PLAYER_ANIM TeamName RewardType ItemID ItemRestType ScenarioEventEndRound hash_types_e)
      string(REGEX MATCH "enum ${regame_type}[^{}]*\\{[^{}]*\\};" regame_enum "${regame_enums}")
      if(regame_enum)
        string(APPEND regame_api_types "${regame_enum}\n")
      endif()
    endforeach()
  endforeach()
  string(FIND "${regame_api_source}" "// CBasePlayer::Spawn hook" regame_hooks_begin)
  math(EXPR regame_hooks_length "${regame_api_begin} - ${regame_hooks_begin}")
  if(regame_hooks_begin LESS 0 OR regame_hooks_length LESS 1)
    message(FATAL_ERROR "Official ReGameDLL hook declarations were not found")
  endif()
  string(SUBSTRING "${regame_api_source}" ${regame_hooks_begin} ${regame_hooks_length} regame_hooks_and_funcs)
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/astrabot_regamedll_api_view.hpp"
    CONTENT "${regame_license}#pragma once\n#include \"hookchains.h\"\n#include \"GameEvent.h\"\nclass Vector;\nclass CBaseEntity;\ntypedef int BOOL;\ntypedef unsigned long ULONG;\ntypedef float vec_t;\ntypedef unsigned int string_t;\ntypedef struct edict_s edict_t;\ntypedef struct entvars_s entvars_t;\n${regame_api_types}\n${regame_hooks_and_funcs}\n${regame_api_declaration}\n${regame_api_defines}" @ONLY)
  add_library(astrabot_regamedll_provider STATIC
    src/adapter/metamod/bridge/regamedll_observation_provider.cpp)
  set_target_properties(astrabot_regamedll_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_include_directories(astrabot_regamedll_provider PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod")
  target_include_directories(astrabot_regamedll_provider SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/dlls"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/regamedll-5.30")
  target_include_directories(astrabot_regamedll_provider SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
  target_link_libraries(astrabot_regamedll_provider PUBLIC ${CMAKE_DL_LIBS})
  astrabot_apply_warnings(astrabot_regamedll_provider)
  foreach(runtime_target astrabot_mm astrabot_metamod_hooks astrabot_fake_client_isolation astrabot_compat_actor_command)
    if(TARGET ${runtime_target})
      target_link_libraries(${runtime_target} PRIVATE astrabot_regamedll_provider)
      if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # The pinned HLSDK Vector has a copy constructor and an implicit copy
        # assignment operator. GCC 14 warns at valid C++14 adapter call sites.
        target_compile_options(${runtime_target} PRIVATE -Wno-deprecated-copy)
      endif()
    endif()
  endforeach()
  if(ASTRABOT_BUILD_TESTS)
    add_executable(astrabot_regamedll_observation_provider tests/regamedll_observation_provider_tests.cpp)
    string(REGEX MATCHALL "virtual IReGameHookRegistry_[A-Za-z0-9_]+ ?\\* ?[A-Za-z0-9_]+\\(\\) = 0;"
      regame_registry_methods "${regame_hooks_and_funcs}")
    set(regame_empty_hookchains "")
    foreach(regame_registry_method IN LISTS regame_registry_methods)
      if(NOT regame_registry_method STREQUAL "")
        string(REGEX REPLACE "^virtual (.*) = 0$" "\\1 override { return nullptr; }"
          regame_empty_hookchain "${regame_registry_method}")
        string(APPEND regame_empty_hookchains "\t${regame_empty_hookchain}\n")
      endif()
    endforeach()
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/astrabot_regamedll_test_hooks.hpp"
      CONTENT "#pragma once\n#include \"astrabot_regamedll_api_view.hpp\"\nclass EmptyReGameHookchains : public IReGameHookchains {\npublic:\n${regame_empty_hookchains}};\n" @ONLY)
    target_include_directories(astrabot_regamedll_observation_provider PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_BINARY_DIR}/generated"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod")
    target_include_directories(astrabot_regamedll_observation_provider SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cssdk/dlls"
      "${CMAKE_CURRENT_SOURCE_DIR}/third_party/regamedll-5.30")
    target_link_libraries(astrabot_regamedll_observation_provider PRIVATE astrabot_regamedll_provider astrabot_core)
    astrabot_apply_warnings(astrabot_regamedll_observation_provider)
    add_test(NAME astrabot_regamedll_observation_provider COMMAND astrabot_regamedll_observation_provider)
  endif()
endif()

if(ASTRABOT_BUILD_TESTS)
  get_property(ASTRABOT_REGISTERED_TESTS DIRECTORY PROPERTY TESTS)
  set_tests_properties(
    ${ASTRABOT_REGISTERED_TESTS}
    PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()

# Private runtime owners are shared by the DLL and adapter integration fixtures.
if(ASTRABOT_BUILD_METAMOD)
  set(ASTRABOT_RUNTIME_OWNER_SOURCES
    src/adapter/metamod/runtime/observation/bot_observation_runtime.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_runtime.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_perception.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_radio.cpp
	src/adapter/metamod/runtime/behavior/bot_behavior_death.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_friend_death.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_voice.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_buying.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_priorities.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_investigation.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_objectives.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_guard.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_equipment.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_obstacles.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_noise.cpp
    src/adapter/metamod/runtime/behavior/bot_look_geometry.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_pickup.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_scenario.cpp
    src/adapter/metamod/runtime/behavior/bot_behavior_combat.cpp
    src/adapter/metamod/runtime/navigation/bot_navigation_runtime.cpp
    src/adapter/metamod/runtime/command/bot_command_runtime.cpp
  )
  foreach(runtime_target astrabot_mm astrabot_metamod_hooks astrabot_fake_client_isolation astrabot_compat_actor_command)
    if(TARGET ${runtime_target})
      target_sources(${runtime_target} PRIVATE ${ASTRABOT_RUNTIME_OWNER_SOURCES})
    endif()
  endforeach()
endif()

# Source ownership and pre-refactor behavior are independent acceptance gates.
if(ASTRABOT_BUILD_TESTS AND ASTRABOT_BUILD_METAMOD)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  foreach(contract source_layout runtime_ports runtime_ownership runtime_movement_integration runtime_behavior runtime_navigation p076_performance)
    add_test(NAME astrabot_${contract}_contract
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/${contract}_contract.py")
  endforeach()
  if(WIN32)
    set(characterization_platform windows-x86)
    find_program(ASTRABOT_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    add_test(NAME astrabot_action_owner_boundary
      COMMAND "${ASTRABOT_POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/phase8_action_adapter_boundary_test.ps1"
        -AdapterPath "${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod/plugin/plugin_runtime.cpp"
        -RequireLiveActions)
  else()
    set(characterization_platform linux-x86)
  endif()
  add_test(NAME astrabot_runtime_characterization
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_characterization_contract.py"
      --executable "$<TARGET_FILE:astrabot_compat_actor_command>"
      --baseline "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/${characterization_platform}.txt.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/actor-generation-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/observed-team-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/native-help-preconditions-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/loose-c4-visibility-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/scenario-deference-cvar-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/preplant-defense-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/kick-all-quota-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/hunt-timing-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/hunt-reachable-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/stuck-scheduling-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/hunt-entry-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/hunt-ladder-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/hide-navigation-look-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/noise-hunt-navigation-look-correction.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/hunt-direct-portal-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/c4-defuse-start-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/c4-move-to-entry-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/c4-approach-look-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/brief-enemy-recognition-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-threat-notice-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-c4-point-vision-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-c4-defuser-cover-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-hunt-approach-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-player-fov-pitch-correction.json"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-fetch-bomb-correction.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-defuse-attack-correction.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-plant-combat-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-combat-range-init-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-plant-terminal-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-clear-look-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-plant-entry-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-move-plant-order-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-idle-navigation-hold-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-idle-buy-look-supply-${characterization_platform}.json.gz"
      --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-shared-look-velocity-${characterization_platform}.json.gz"
        --correction "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/runtime_characterization/csbot-full-look-publication-${characterization_platform}.json.gz"
      --platform "${characterization_platform}"
      --output "${CMAKE_CURRENT_BINARY_DIR}/runtime-characterization.txt")
  add_test(NAME astrabot_runtime_characterization_checker
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_characterization_checker_tests.py")
  # This fixture and astrabot_compat_actor_command temporarily create the same
  # profile files; never execute them concurrently in the source directory.
  set_tests_properties(astrabot_compat_actor_command astrabot_runtime_characterization PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_LOCK astrabot_actor_command_profile)
endif()

if(ASTRABOT_BUILD_TESTS AND ASTRABOT_BUILD_METAMOD)
  get_target_property(combat_target_log_sources astrabot_compat_actor_command SOURCES)
  list(REMOVE_ITEM combat_target_log_sources tests/compat_actor_command_tests.cpp)
  add_executable(astrabot_combat_target_log tests/combat_target_log_tests.cpp ${combat_target_log_sources})
  get_target_property(combat_target_log_includes astrabot_compat_actor_command INCLUDE_DIRECTORIES)
  target_include_directories(astrabot_combat_target_log PRIVATE ${combat_target_log_includes})
  add_executable(astrabot_movement_sound_bridge tests/movement_sound_bridge_tests.cpp)
  target_include_directories(astrabot_movement_sound_bridge PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_movement_sound_bridge PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_movement_sound_bridge)
  add_test(NAME astrabot_movement_sound_bridge COMMAND astrabot_movement_sound_bridge)
  target_link_libraries(astrabot_combat_target_log PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_combat_target_log)
  add_test(NAME astrabot_combat_target_log COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_threatening_gunfire COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_reacquire_delay COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_occluded_acquisition COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_self_defense COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_shield COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_best_equipment COMMAND astrabot_combat_target_log --target-log)
    add_test(NAME astrabot_plant_terminal_order COMMAND astrabot_combat_target_log --target-log)
    add_test(NAME astrabot_clear_look_upkeep COMMAND astrabot_combat_target_log --target-log)
    add_test(NAME astrabot_shared_look_velocity COMMAND astrabot_combat_target_log --target-log)
    add_test(NAME astrabot_shared_look_reentry COMMAND astrabot_combat_target_log --target-log)
    add_test(NAME astrabot_native_grenade_lifecycle COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_plant_entry_order COMMAND astrabot_combat_target_log --target-log)
  add_test(NAME astrabot_combat_threat_notice COMMAND astrabot_combat_target_log --target-log)
  add_executable(astrabot_bomb_guard_look tests/bomb_guard_look_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_bomb_guard_look PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_bomb_guard_look PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_bomb_guard_look)
  add_test(NAME astrabot_bomb_guard_look COMMAND astrabot_bomb_guard_look --target-log)
  add_executable(astrabot_weapon_switch_runtime tests/weapon_switch_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_weapon_switch_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_weapon_switch_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_weapon_switch_runtime)
  add_test(NAME astrabot_weapon_switch_runtime COMMAND astrabot_weapon_switch_runtime --target-log)
  add_executable(astrabot_actor_identity_runtime tests/actor_identity_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_actor_identity_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_actor_identity_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_actor_identity_runtime)
  add_test(NAME astrabot_actor_identity_runtime COMMAND astrabot_actor_identity_runtime --target-log)
  add_executable(astrabot_runtime_team_resolution tests/runtime_team_resolution_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_runtime_team_resolution PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_runtime_team_resolution PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_runtime_team_resolution)
  add_test(NAME astrabot_runtime_team_resolution COMMAND astrabot_runtime_team_resolution --target-log)
  set_tests_properties(astrabot_runtime_team_resolution PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  set_tests_properties(astrabot_actor_identity_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  set_tests_properties(astrabot_weapon_switch_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  set_tests_properties(astrabot_bomb_guard_look PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  set_tests_properties(astrabot_combat_threatening_gunfire PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=gunfire")
  set_tests_properties(astrabot_combat_reacquire_delay PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=reacquire")
  set_tests_properties(astrabot_combat_occluded_acquisition PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=occluded")
  set_tests_properties(astrabot_combat_self_defense PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=self-defense")
  set_tests_properties(astrabot_combat_shield PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=shield")
  set_tests_properties(astrabot_combat_best_equipment PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=equip-best")
    set_tests_properties(astrabot_plant_terminal_order PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=plant-tail")
    set_tests_properties(astrabot_clear_look_upkeep PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=clear-look")
    set_tests_properties(astrabot_shared_look_velocity PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=shared-look" RUN_SERIAL TRUE)
    set_tests_properties(astrabot_shared_look_reentry PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=shared-look-reentry" RUN_SERIAL TRUE)
    set_tests_properties(astrabot_native_grenade_lifecycle PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=native-grenade" RUN_SERIAL TRUE)
  set_tests_properties(astrabot_plant_entry_order PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=plant-entry")
  set_tests_properties(astrabot_combat_threat_notice PROPERTIES ENVIRONMENT "ASTRABOT_TEST_COMBAT_SCENARIO=notice")
  set_tests_properties(astrabot_combat_target_log astrabot_combat_threatening_gunfire astrabot_combat_reacquire_delay
    astrabot_combat_threat_notice astrabot_combat_occluded_acquisition astrabot_combat_self_defense astrabot_combat_shield
    astrabot_combat_best_equipment astrabot_plant_terminal_order astrabot_clear_look_upkeep astrabot_shared_look_velocity astrabot_shared_look_reentry astrabot_native_grenade_lifecycle astrabot_plant_entry_order PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_obstacle_interaction_runtime tests/obstacle_interaction_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_obstacle_interaction_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_obstacle_interaction_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_obstacle_interaction_runtime)
  add_test(NAME astrabot_obstacle_interaction_runtime COMMAND astrabot_obstacle_interaction_runtime --obstacles)
  set_tests_properties(astrabot_obstacle_interaction_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_noise_investigation_runtime tests/noise_investigation_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_noise_investigation_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_noise_investigation_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_noise_investigation_runtime)
  add_test(NAME astrabot_noise_investigation_runtime COMMAND astrabot_noise_investigation_runtime --investigation)
  set_tests_properties(astrabot_noise_investigation_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_statement_queue_runtime tests/statement_queue_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_statement_queue_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_statement_queue_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_statement_queue_runtime)
  add_test(NAME astrabot_statement_queue_runtime COMMAND astrabot_statement_queue_runtime --statement-queue)
  add_test(NAME astrabot_reporting_in_runtime COMMAND astrabot_statement_queue_runtime --reporting-in)
  add_test(NAME astrabot_bombsite_clear_runtime COMMAND astrabot_statement_queue_runtime --bombsite-clear)
  add_test(NAME astrabot_bombsite_planted_runtime COMMAND astrabot_statement_queue_runtime --bombsite-planted)
  add_test(NAME astrabot_bombsite_request_runtime COMMAND astrabot_statement_queue_runtime --bombsite-request)
  add_test(NAME astrabot_bomb_carrier_runtime COMMAND astrabot_statement_queue_runtime --bomb-carrier)
  add_test(NAME astrabot_loose_bomb_runtime COMMAND astrabot_statement_queue_runtime --loose-bomb)
  add_test(NAME astrabot_guard_loose_bomb_runtime COMMAND astrabot_statement_queue_runtime --guard-loose-bomb)
  add_test(NAME astrabot_guard_planted_bombsite_runtime COMMAND astrabot_statement_queue_runtime --guard-planted-bombsite)
  add_test(NAME astrabot_encourage_defuse_runtime COMMAND astrabot_statement_queue_runtime --encourage-defuse)
  add_test(NAME astrabot_guard_unplanted_bombsite_runtime COMMAND astrabot_statement_queue_runtime --guard-unplanted-bombsite)
  add_test(NAME astrabot_waiting_here_runtime COMMAND astrabot_statement_queue_runtime --waiting-here)
  add_test(NAME astrabot_hostage_use_report_runtime COMMAND astrabot_statement_queue_runtime --hostage-use-report)
  add_test(NAME astrabot_sniper_warning_runtime COMMAND astrabot_statement_queue_runtime --sniper-warning)
  add_test(NAME astrabot_scared_outnumbered_runtime COMMAND astrabot_statement_queue_runtime --scared-outnumbered)
  add_test(NAME astrabot_team_fall_back_runtime COMMAND astrabot_statement_queue_runtime --team-fall-back)
  add_test(NAME astrabot_round_victory_runtime COMMAND astrabot_statement_queue_runtime --round-victory)
  add_test(NAME astrabot_barely_defused_runtime COMMAND astrabot_statement_queue_runtime --barely-defused)
  add_test(NAME astrabot_blinded_chatter_runtime COMMAND astrabot_statement_queue_runtime --blinded-chatter)
  add_test(NAME astrabot_friendly_fire_chatter_runtime COMMAND astrabot_statement_queue_runtime --friendly-fire-chatter)
  add_test(NAME astrabot_enemy_damage_hide_runtime COMMAND astrabot_statement_queue_runtime --enemy-damage-hide)
  add_test(NAME astrabot_damage_retarget_runtime COMMAND astrabot_statement_queue_runtime --damage-retarget)
  add_test(NAME astrabot_auto_follow_chatter_runtime COMMAND astrabot_statement_queue_runtime --auto-follow-chatter)
  set_tests_properties(astrabot_statement_queue_runtime astrabot_guard_loose_bomb_runtime
    astrabot_guard_planted_bombsite_runtime astrabot_encourage_defuse_runtime astrabot_guard_unplanted_bombsite_runtime
    astrabot_hostage_use_report_runtime astrabot_sniper_warning_runtime astrabot_scared_outnumbered_runtime
    astrabot_team_fall_back_runtime astrabot_round_victory_runtime astrabot_auto_follow_chatter_runtime
    astrabot_waiting_here_runtime astrabot_barely_defused_runtime astrabot_blinded_chatter_runtime
    astrabot_friendly_fire_chatter_runtime astrabot_enemy_damage_hide_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_chatter_nav_probe tests/chatter_nav_probe_tests.cpp)
  target_include_directories(astrabot_chatter_nav_probe PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_chatter_nav_probe PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_chatter_nav_probe)
  add_test(NAME astrabot_chatter_nav_probe COMMAND astrabot_chatter_nav_probe)
  add_executable(astrabot_compound_chatter_runtime tests/compound_chatter_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_compound_chatter_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_compound_chatter_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_compound_chatter_runtime)
  add_test(NAME astrabot_compound_chatter_runtime COMMAND astrabot_compound_chatter_runtime --compound)
  add_test(NAME astrabot_chatter_bank_load_round COMMAND astrabot_compound_chatter_runtime --compound)
  add_test(NAME astrabot_chatter_bank_free_round COMMAND astrabot_compound_chatter_runtime --compound)
  set_tests_properties(astrabot_chatter_bank_load_round PROPERTIES ENVIRONMENT "ASTRABOT_TEST_BANK_RESET=load" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  set_tests_properties(astrabot_chatter_bank_free_round PROPERTIES ENVIRONMENT "ASTRABOT_TEST_BANK_RESET=free" WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  set_tests_properties(astrabot_compound_chatter_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_death_interrupt_runtime tests/death_interrupt_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_death_interrupt_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_death_interrupt_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_death_interrupt_runtime)
  add_test(NAME astrabot_death_interrupt_runtime COMMAND astrabot_death_interrupt_runtime --death-chatter)
  add_test(NAME astrabot_death_interrupt_default_catalog COMMAND astrabot_death_interrupt_runtime --death-chatter-default-missing)
  set_tests_properties(astrabot_death_interrupt_runtime PROPERTIES ENVIRONMENT "ASTRABOT_TEST_DEATH_DEFAULT_MISSING=0")
  set_tests_properties(astrabot_death_interrupt_default_catalog PROPERTIES ENVIRONMENT "ASTRABOT_TEST_DEATH_DEFAULT_MISSING=1")
  add_test(NAME astrabot_death_interrupt_missing_sample COMMAND astrabot_death_interrupt_runtime --missing-sample)
  set_tests_properties(astrabot_death_interrupt_missing_sample PROPERTIES ENVIRONMENT "ASTRABOT_TEST_DEATH_DEFAULT_MISSING=2")
  set_tests_properties(astrabot_death_interrupt_runtime astrabot_death_interrupt_default_catalog astrabot_death_interrupt_missing_sample PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_friend_death_chatter_runtime tests/friend_death_chatter_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_friend_death_chatter_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_friend_death_chatter_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_friend_death_chatter_runtime)
  add_test(NAME astrabot_friend_death_chatter_runtime COMMAND astrabot_friend_death_chatter_runtime --death-chatter)
  add_test(NAME astrabot_friend_death_chatter_default_catalog COMMAND astrabot_friend_death_chatter_runtime --death-chatter-default-missing)
  set_tests_properties(astrabot_friend_death_chatter_runtime PROPERTIES ENVIRONMENT "ASTRABOT_TEST_DEATH_DEFAULT_MISSING=0")
  set_tests_properties(astrabot_friend_death_chatter_default_catalog PROPERTIES ENVIRONMENT "ASTRABOT_TEST_DEATH_DEFAULT_MISSING=1")
  set_tests_properties(astrabot_friend_death_chatter_runtime astrabot_friend_death_chatter_default_catalog PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_friend_death_runtime tests/friend_death_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_friend_death_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_friend_death_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_friend_death_runtime)
  add_test(NAME astrabot_friend_death_runtime COMMAND astrabot_friend_death_runtime --friend-death)
  set_tests_properties(astrabot_friend_death_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_kill_praise_runtime tests/kill_praise_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_kill_praise_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_kill_praise_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_kill_praise_runtime)
  add_test(NAME astrabot_kill_praise_runtime COMMAND astrabot_kill_praise_runtime --kill-praise)
  add_test(NAME astrabot_kill_praise_default_catalog COMMAND astrabot_kill_praise_runtime --praise-default-missing)
  set_tests_properties(astrabot_kill_praise_runtime PROPERTIES
    ENVIRONMENT "ASTRABOT_TEST_PRAISE_DEFAULT_MISSING=0")
  set_tests_properties(astrabot_kill_praise_default_catalog PROPERTIES
    ENVIRONMENT "ASTRABOT_TEST_PRAISE_DEFAULT_MISSING=1")
  set_tests_properties(astrabot_kill_praise_runtime astrabot_kill_praise_default_catalog PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_enemy_count_chatter_runtime tests/enemy_count_chatter_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_enemy_count_chatter_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_enemy_count_chatter_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_enemy_count_chatter_runtime)
  add_test(NAME astrabot_enemy_count_chatter_runtime COMMAND astrabot_enemy_count_chatter_runtime --enemy-count)
  set_tests_properties(astrabot_enemy_count_chatter_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_pm_move_footsteps tests/pm_move_footstep_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_pm_move_footsteps PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_pm_move_footsteps PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_pm_move_footsteps)
  add_test(NAME astrabot_pm_move_footsteps COMMAND astrabot_pm_move_footsteps --pm-move)
  set_tests_properties(astrabot_pm_move_footsteps PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_gunshot_runtime tests/gunshot_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_gunshot_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_gunshot_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_gunshot_runtime)
  add_test(NAME astrabot_gunshot_runtime COMMAND astrabot_gunshot_runtime --gunshot)
  set_tests_properties(astrabot_gunshot_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_noise_look_runtime tests/noise_look_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_noise_look_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_noise_look_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_noise_look_runtime)
  add_test(NAME astrabot_noise_look_runtime COMMAND astrabot_noise_look_runtime --noise-look)
  set_tests_properties(astrabot_noise_look_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_chatter_voice_runtime tests/chatter_voice_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_chatter_voice_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_chatter_voice_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_chatter_voice_runtime)
  add_test(NAME astrabot_chatter_voice_runtime COMMAND astrabot_chatter_voice_runtime --voice)
  set_tests_properties(astrabot_chatter_voice_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_weapon_pickup_runtime tests/weapon_pickup_runtime_tests.cpp ${combat_target_log_sources})
  target_include_directories(astrabot_weapon_pickup_runtime PRIVATE ${combat_target_log_includes})
  target_link_libraries(astrabot_weapon_pickup_runtime PRIVATE astrabot_core astrabot_regamedll_provider)
  astrabot_apply_warnings(astrabot_weapon_pickup_runtime)
  add_test(NAME astrabot_weapon_pickup_runtime COMMAND astrabot_weapon_pickup_runtime --pickup)
  set_tests_properties(astrabot_weapon_pickup_runtime PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" RESOURCE_LOCK astrabot_actor_command_profile)
  add_executable(astrabot_runtime_command_scenarios
    tests/runtime_command_scenarios.cpp
    src/adapter/metamod/config/compat_surface.cpp
    src/adapter/metamod/config/profile_loader.cpp
    src/adapter/metamod/lifecycle/join_controller.cpp
    src/adapter/metamod/runtime/command/bot_command_runtime.cpp
    src/adapter/metamod/bridge/input_dispatcher.cpp
    src/adapter/metamod/diagnostics/runtime_profiler.cpp
    src/adapter/metamod/navigation/movement_execution_gate.cpp
    src/adapter/metamod/bridge/action_adapter.cpp)
  target_include_directories(astrabot_runtime_command_scenarios PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adapter/metamod
    ${ASTRABOT_METAMOD_INCLUDE_DIRS})
  target_include_directories(astrabot_runtime_command_scenarios SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
  target_link_libraries(astrabot_runtime_command_scenarios PRIVATE astrabot_core)
  astrabot_apply_warnings(astrabot_runtime_command_scenarios)
  add_test(NAME astrabot_runtime_command_scenarios COMMAND astrabot_runtime_command_scenarios)
  add_test(NAME astrabot_runtime_architecture_checker
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_architecture_checker_tests.py)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # These fixtures share the pinned HLSDK Vector and runtime sources with
    # astrabot_compat_actor_command; retain its existing GCC SDK exception.
    foreach(runtime_fixture astrabot_combat_target_log astrabot_bomb_guard_look
        astrabot_weapon_switch_runtime astrabot_actor_identity_runtime
        astrabot_runtime_team_resolution astrabot_runtime_command_scenarios astrabot_obstacle_interaction_runtime
        astrabot_chatter_nav_probe astrabot_compound_chatter_runtime astrabot_statement_queue_runtime astrabot_weapon_pickup_runtime astrabot_chatter_voice_runtime astrabot_noise_look_runtime astrabot_gunshot_runtime astrabot_noise_investigation_runtime astrabot_pm_move_footsteps astrabot_enemy_count_chatter_runtime astrabot_kill_praise_runtime astrabot_friend_death_runtime astrabot_friend_death_chatter_runtime astrabot_death_interrupt_runtime)
      target_compile_options(${runtime_fixture} PRIVATE -Wno-deprecated-copy)
    endforeach()
  endif()
endif()
