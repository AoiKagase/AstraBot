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
    assert (
        'if (needsBombSite && std::strcmp(classname, "func_bomb_target") == 0)'
        in body
    )


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


if __name__ == "__main__":
    test_non_objective_bots_skip_bomb_site_corridor_enumeration()
    test_diagnostic_performance_cvars_are_present_and_default_off()
    test_objective_sensor_exposes_internal_path_stats()
    test_objective_site_selection_uses_one_representative_nav_area()
    test_path_recompute_updates_the_profiler_stage()
    test_objective_target_is_cached_for_unchanged_objective_state()
    print("p076 performance contract: PASS")
