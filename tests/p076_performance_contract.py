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


if __name__ == "__main__":
    test_non_objective_bots_skip_bomb_site_corridor_enumeration()
    test_diagnostic_performance_cvars_are_present_and_default_off()
    test_objective_sensor_exposes_internal_path_stats()
    print("p076 performance contract: PASS")
