from pathlib import Path


def test_non_objective_bots_skip_bomb_site_corridor_enumeration() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    start = source.index("bool PluginRuntime::buildManagedObjectiveTarget(")
    end = source.index("ActionProposal PluginRuntime::decideManagedBotAction(", start)
    body = source[start:end]
    assert "const bool needsBombSite = team == 1 && carryingBomb;" in body
    assert "const bool needsPlantedBomb = team == 2;" in body
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
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    assert "nav::NavSearchStats *searchStats" in source
    assert "objectiveSearchStats" in source
    assert "addNavSearchStats(searchStats" in source
    assert "pathSearchTotalUsec" in source


def test_objective_site_selection_uses_one_representative_nav_area() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    start = source.index("bool PluginRuntime::buildManagedObjectiveTarget(")
    end = source.index("ActionProposal PluginRuntime::decideManagedBotAction(", start)
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
    start = source.index("void RuntimeProfiler::recordPathRecompute()")
    end = source.index("void RuntimeProfiler::recordRunPlayerMove()", start)
    body = source[start:end]
    assert "RuntimeProfilerStage::PathRecompute" in body


def test_objective_target_is_cached_for_unchanged_objective_state() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    assert "managedBotObjectiveTargets_" in source
    assert "cache.mapGeneration == lifecycle_.mapGeneration()" in source
    assert "cache.roundGeneration == lifecycle_.roundGeneration()" in source
    assert "cache.carryingBomb == carryingBomb" in source
    assert "cache.selectedEntityIndex" in source


def test_bomb_target_enumeration_preserves_legacy_entities_and_identity() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    start = source.index("bool PluginRuntime::buildManagedObjectiveTarget(")
    end = source.index(
        "ActionProposal PluginRuntime::decideManagedBotAction(", start
    )
    body = source[start:end]
    assert "isBombTargetClassname(classname)" in body
    assert 'std::strcmp(classname, "func_bomb_target") == 0' in source
    assert 'std::strcmp(classname, "info_bomb_target") == 0' in source
    assert "selectedEntityIndex" in body
    assert "siteIdentity" in body


def test_bomb_target_candidates_do_not_collapse_distinct_sites_by_area() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    start = source.index("bool PluginRuntime::buildManagedObjectiveTarget(")
    end = source.index(
        "ActionProposal PluginRuntime::decideManagedBotAction(", start
    )
    body = source[start:end]
    assert "ObjectiveCandidateKey" in body
    assert "entityIndex" in body
    assert "targetArea" in source
    assert "corridor.cost" in body
    assert "pathCost < selectedPathCost" in body
    assert "target_id" in source
    assert "nearest_nav_area" in source
    assert "rejection_reason" in source


def test_goal_assignment_trace_keeps_actor_local_cache_scope() -> None:
    root = Path(__file__).parents[1]
    runtime_source = (
        root / "src" / "adapter" / "metamod" / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    runtime_header = (
        root / "src" / "adapter" / "metamod" / "plugin_runtime.hpp"
    ).read_text(encoding="utf-8")
    assert "logGoalAssignmentDiagnostic" in runtime_source
    assert "lastCacheHit" in runtime_source
    assert "cache_scope=actor" in runtime_source
    assert "std::array<runtime::NavRoamController" in runtime_header
    assert "std::array<ManagedObjectiveTargetCache" in runtime_header


def test_traversal_trace_exposes_jump_execution_chain() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
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
    source = (
        root / "src" / "adapter" / "metamod" / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    assert "resolveManagedBotTeam" in source
    state_start = source.index(
        "void PluginRuntime::updateManagedBotCompatibilityState("
    )
    state_body = source[state_start:]
    assert "resolveManagedBotTeam" in state_body
    objective_start = source.index(
        "bool PluginRuntime::buildManagedObjectiveTarget("
    )
    objective_body = source[objective_start:]
    assert "resolveManagedBotTeam" in objective_body
    action_start = source.index("ActionProposal PluginRuntime::decideManagedBotAction(")
    action_body = source[action_start:]
    assert "resolveManagedBotTeam" in action_body


def test_runtime_diagnostic_separates_raw_and_effective_team() -> None:
    source = (
        Path(__file__).parents[1]
        / "src"
        / "adapter"
        / "metamod"
        / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    for field in ("raw_team", "effective_team", "team_info", "team_fresh"):
        assert field in source


def test_bomb_target_registry_separates_registration_from_evaluation() -> None:
    root = Path(__file__).parents[1]
    runtime_source = (
        root / "src" / "adapter" / "metamod" / "plugin_runtime.cpp"
    ).read_text(encoding="utf-8")
    runtime_header = (
        root / "src" / "adapter" / "metamod" / "plugin_runtime.hpp"
    ).read_text(encoding="utf-8")
    profiler_header = (
        root / "include" / "astrabot" / "metamod" / "runtime_profiler.hpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "ManagedObjectiveSiteRegistry",
        "refreshManagedObjectiveSiteRegistry",
        "registered_site_count",
        "evaluated_this_window",
        "selected_site_id",
    ):
        assert symbol in runtime_source or symbol in runtime_header
    assert "objectiveRegisteredSites" in profiler_header
    assert "objectiveCacheHits" in profiler_header


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
    print("p076 performance contract: PASS")
