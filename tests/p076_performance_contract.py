from pathlib import Path
import re

def normalize(source):
    # Formatting may wrap C++ tokens and concatenate adjacent log literals.
    return re.sub(r"\s+", " ", re.sub(r'"\s*"', '', source))

ROOT = Path(__file__).parents[1]
ADAPTER = ROOT / "src/adapter/metamod"
def behavior_source():
    return "\n".join((ADAPTER / ("bot_behavior_" + name + ".cpp")).read_text(encoding="utf-8") for name in ("objectives", "runtime", "scenario", "combat", "buying", "radio"))

def adapter_source(owner):
    return behavior_source() if owner == "behavior" else (ADAPTER / ("bot_" + owner + "_runtime.cpp")).read_text(encoding="utf-8")
def adapter_header(owner):
    return (ADAPTER / ("bot_" + owner + "_runtime.hpp")).read_text(encoding="utf-8")
def adapter_support():
    return (ADAPTER / "bot_runtime_support.hpp").read_text(encoding="utf-8")
def adapter_types():
    return (ADAPTER / "bot_runtime_types.hpp").read_text(encoding="utf-8")
def adapter_facade():
    return (ADAPTER / "plugin_runtime.cpp").read_text(encoding="utf-8")



def test_non_objective_bots_skip_bomb_site_corridor_enumeration() -> None:
    source = behavior_source()
    source = normalize(source)
    start = source.index("bool BotBehaviorRuntime::buildManagedObjectiveTarget(")
    end = source.index("ActionProposal BotBehaviorRuntime::decideManagedBotAction(", start)
    body = source[start:end]
    assert "const bool needsBombSite = team == 1 && carryingBomb &&" in body
    assert "assignmentKind == team::TeamObjectiveKind::PlantC4;" in body
    assert "const bool needsPlantedBomb = team == 2 &&" in body
    assert "assignmentKind == team::TeamObjectiveKind::DefuseC4;" in body
    assert "if (!needsBombSite && !needsPlantedBomb)" in body
    assert "isBombTargetClassname(classname)" in body


def test_diagnostic_performance_cvars_are_present_and_default_off() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    source = normalize(source)
    for name in (
        "astrabot_perf_disable_vision",
        "astrabot_perf_disable_pathsearch",
        "astrabot_perf_disable_trace",
    ):
        assert name in source
    assert 'char kAstrabotPerfDisableVisionDefault[] = "0";' in source
    assert 'char kAstrabotPerfDisablePathSearchDefault[] = "0";' in source
    assert 'char kAstrabotPerfDisableTraceDefault[] = "0";' in source


def test_objective_sensor_exposes_internal_path_stats() -> None:
    source = behavior_source()
    source = normalize(source)
    assert "nav::NavSearchStats *searchStats" in source
    assert "objectiveSearchStats" in adapter_source("navigation")
    assert "addNavSearchStats(searchStats" in source
    assert "pathSearchTotalUsec" in adapter_facade()


def test_objective_site_selection_uses_one_representative_nav_area() -> None:
    source = behavior_source()
    source = normalize(source)
    start = source.index("bool BotBehaviorRuntime::buildManagedObjectiveTarget(")
    end = source.index("ActionProposal BotBehaviorRuntime::decideManagedBotAction(", start)
    body = source[start:end]
    assert "for (const nav::NavArea &area : document->areas())" not in body
    assert "query.findNearest(" in body


def test_path_recompute_updates_the_profiler_stage() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "runtime_profiler.cpp"
    ).read_text(encoding="utf-8")
    source = normalize(source)
    start = source.index("void RuntimeProfiler::recordPathRecompute()")
    end = source.index("void RuntimeProfiler::recordRunPlayerMove()", start)
    body = source[start:end]
    assert "RuntimeProfilerStage::PathRecompute" in body


def test_objective_target_is_cached_for_unchanged_objective_state() -> None:
    source = behavior_source()
    source = normalize(source)
    assert "managedBotObjectiveTargets" in source
    assert "cache.mapGeneration == services_->service_lifecycle().mapGeneration()" in source
    assert "cache.roundGeneration == services_->service_lifecycle().roundGeneration()" in source
    assert "cache.carryingBomb == carryingBomb" in source
    assert "cache.selectedEntityIndex" in source


def test_bomb_target_enumeration_preserves_legacy_entities_and_identity() -> None:
    source = behavior_source()
    source = normalize(source)
    start = source.index("bool BotBehaviorRuntime::buildManagedObjectiveTarget(")
    end = source.index(
        "ActionProposal BotBehaviorRuntime::decideManagedBotAction(", start
    )
    body = source[start:end]
    assert "isBombTargetClassname(classname)" in body
    action_source = (ADAPTER / "action_adapter.cpp").read_text(encoding="utf-8")
    assert 'equalsIgnoreCase(classname, "func_bomb_target")' in action_source
    assert 'equalsIgnoreCase(classname, "info_bomb_target")' in action_source
    assert "selectedEntityIndex" in body
    assert "siteIdentity" in body


def test_bomb_target_candidates_do_not_collapse_distinct_sites_by_area() -> None:
    source = behavior_source()
    source = normalize(source)
    start = source.index("bool BotBehaviorRuntime::buildManagedObjectiveTarget(")
    end = source.index(
        "ActionProposal BotBehaviorRuntime::decideManagedBotAction(", start
    )
    body = source[start:end]
    assert "ObjectiveCandidateKey" in body
    assert "entityIndex" in body
    assert "targetArea" in source
    assert "corridor.cost" in body
    assert "objectives::isBetterBombSiteRoute" in body
    assert "target_id" in adapter_support()
    assert "nearest_nav_area" in adapter_support()
    assert "rejection_reason" in adapter_support()


def test_goal_assignment_trace_keeps_actor_local_cache_scope() -> None:
    root = Path(__file__).parents[1]
    runtime_source = (
        root / "src" / "adapter" / "metamod" / "bot_navigation_runtime.cpp"
    ).read_text(encoding="utf-8")
    runtime_source = normalize(runtime_source)
    runtime_header = (
        root / "src" / "adapter" / "metamod" / "bot_navigation_runtime.hpp"
    ).read_text(encoding="utf-8")
    runtime_header = normalize(runtime_header)
    assert "logGoalAssignmentDiagnostic" in runtime_source
    assert "lastCacheHit" in runtime_source
    assert "cache_scope=actor" in runtime_source
    assert "runtime::NavRoamController managedBotMovement" in runtime_header
    assert "ManagedObjectiveTargetCache managedBotObjectiveTargets" in adapter_header("behavior")


def test_traversal_trace_exposes_jump_execution_chain() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "bot_navigation_runtime.cpp"
    ).read_text(encoding="utf-8")
    source = normalize(source)
    assert "logTraversalDiagnostic" in source
    for field in (
        "connection_how",
        "current_attributes",
        "next_attributes",
        "traversal_intent",
        "jump_required",
        "velocity_z",
        "IN_JUMP",
        "corridor_index",
    ):
        assert field in source


def test_effective_team_is_shared_across_runtime_decisions() -> None:
    root = Path(__file__).parents[1]
    source = behavior_source()
    source = normalize(source)
    assert "resolveManagedBotTeam" in source
    state_start = source.index(
        "void BotBehaviorRuntime::updateManagedBotCompatibilityState("
    )
    state_body = source[state_start:]
    assert "resolveManagedBotTeam" in state_body
    objective_start = source.index(
        "bool BotBehaviorRuntime::buildManagedObjectiveTarget("
    )
    objective_body = source[objective_start:]
    assert "resolveManagedBotTeam" in objective_body
    action_start = source.index("ActionProposal BotBehaviorRuntime::decideManagedBotAction(")
    action_body = source[action_start:]
    assert "resolveManagedBotTeam" in action_body


def test_runtime_diagnostic_separates_raw_and_effective_team() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "bot_navigation_runtime.cpp"
    ).read_text(encoding="utf-8")
    source = normalize(source)
    for field in ("raw_team", "effective_team", "team_info", "team_fresh"):
        assert field in source


def test_bomb_target_registry_separates_registration_from_evaluation() -> None:
    root = Path(__file__).parents[1]
    runtime_source = behavior_source()
    runtime_source = normalize(runtime_source)
    runtime_header = (
        root / "src" / "adapter" / "metamod" / "bot_behavior_runtime.hpp"
    ).read_text(encoding="utf-8")
    runtime_header = normalize(runtime_header)
    profiler_header = (
        root / "include" / "astrabot" / "metamod" / "runtime_profiler.hpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "ManagedObjectiveSiteRegistry",
        "refreshManagedObjectiveSiteRegistry",
        "registered_site_count",
        "registered_func_bomb_target_count",
        "registered_info_bomb_target_count",
        "evaluated_this_window",
        "selected_site_id",
    ):
        assert symbol in runtime_source or symbol in adapter_types() or symbol in adapter_source("navigation") or symbol in adapter_support() or symbol in adapter_facade()
    assert "objectiveRegisteredSites" in profiler_header
    assert "objectiveCacheHits" in profiler_header


def test_bomb_site_diagnostics_separate_site_type_cost_and_cache_state() -> None:
    root = Path(__file__).parents[1]
    runtime_source = behavior_source()
    runtime_source = normalize(runtime_source)
    profiler_header = (
        root / "include" / "astrabot" / "metamod" / "runtime_profiler.hpp"
    ).read_text(encoding="utf-8")
    for field in (
        "evaluated_func_bomb_target_count=%llu",
        "evaluated_info_bomb_target_count=%llu",
        "registered_site_count=%u",
        "registered_func_bomb_target_count=%u",
        "registered_info_bomb_target_count=%u",
        "candidate_area_samples=%u",
        "unique_nav_candidate_areas=%u",
        "reachable_nav_candidate_areas=%u",
        "best_reachable_cost=%.1f",
        "cache_state=%s",
        "rejection_reason=%s",
    ):
        assert field in runtime_source or field in adapter_support() or field in adapter_facade()
    assert "objectives::isBetterBombSiteRoute" in runtime_source
    assert "objectiveFuncBombTargetSites" in profiler_header
    assert "objectiveInfoBombTargetSites" in profiler_header


def test_goal_and_traversal_diagnostics_correlate_post_move_feedback() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "bot_navigation_runtime.cpp"
    ).read_text(encoding="utf-8")
    source = normalize(source)
    for field in (
        "goalAssignmentCorrelation",
        "frame=%u nav_update=%u goal_generation=%u reselection_reason=%s",
        "selection_strategy=%s",
        "objective_generation=%u selected_site_id=%u",
        "nav_result=%d",
        "path_result=%d",
        "failure_reason=%d",
        "path_search_calls=%u path_search_expanded=%u path_search_enqueues=%u path_search_failures=%u",
        "profile traversalCorrelation",
        "profile traversalResult",
        "after_move=(%.1f %.1f %.1f)",
        "grounded_after=%d",
        "health_delta=%.1f",
    ):
        assert field in source


def test_invalid_corridor_records_failure_and_avoids_objective_link() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "core"
        / "runtime"
        / "nav_roam_controller.cpp"
    ).read_text(encoding="utf-8")
    source = normalize(source)
    invalid_corridor = source.split(
        "case nav::LocomotionResult::InvalidCorridor:", 1
    )[1].split("case nav::LocomotionResult::InvalidArgument:", 1)[0]
    assert "decision->failureReason = NavFailureReason::NavApplyRejected;" in invalid_corridor
    assert "routeState().hasAvoidedLink = true;" in invalid_corridor
    assert "routeState().avoidedLink = routeState().activeLink;" in invalid_corridor
    route_selection = source.split(
        "bool NavRoamController::selectRoute(", 1
    )[1].split("bool NavRoamController::selectCompatibilityGoal(", 1)[0]
    assert "buildAlternativeCorridor" in route_selection
    assert "startRoute" in source
    assert "portalSteeringPointForLink" in source
    assert "isWithinTraversalLaunchTolerance" in source
    objective_change = source.split("if (objectiveTargetChanged)", 1)[1].split(
        "roamGoalState().hasObjectiveTarget =", 1
    )[0]
    assert "clearPathFailure();" in objective_change
    assert "routeState().hasAvoidedLink = false;" in objective_change


if __name__ == "__main__":
    test_non_objective_bots_skip_bomb_site_corridor_enumeration()
    test_diagnostic_performance_cvars_are_present_and_default_off()
    test_objective_sensor_exposes_internal_path_stats()
    test_objective_site_selection_uses_one_representative_nav_area()
    test_path_recompute_updates_the_profiler_stage()
    test_objective_target_is_cached_for_unchanged_objective_state()
    test_bomb_target_enumeration_preserves_legacy_entities_and_identity()
    test_bomb_target_candidates_do_not_collapse_distinct_sites_by_area()
    test_goal_assignment_trace_keeps_actor_local_cache_scope()
    test_traversal_trace_exposes_jump_execution_chain()
    test_effective_team_is_shared_across_runtime_decisions()
    test_runtime_diagnostic_separates_raw_and_effective_team()
    test_bomb_target_registry_separates_registration_from_evaluation()
    test_bomb_site_diagnostics_separate_site_type_cost_and_cache_state()
    test_goal_and_traversal_diagnostics_correlate_post_move_feedback()
    test_invalid_corridor_records_failure_and_avoids_objective_link()
    print("p076 performance contract: PASS")
