from pathlib import Path


ROOT = Path(__file__).parents[1]


def test_p076_runtime_model_is_explicit():
    model = (ROOT / "docs" / "parity" / "PRODUCTION_RUNTIME_MODEL.md").read_text(
        encoding="utf-8"
    )
    for required in (
        "StartFrame",
        "onStartFramePost",
        "BotTimingScheduler",
        "authoritative",
        "maxspeed",
        "Goal producer",
        "CurrentAreaMissing",
        "PathSearchFailed",
        "NavApplyRejected",
        "TraceLine/sec",
        "pathSearch/sec",
        "astrabot_profile",
        "P07.6-PERF execution inventory",
        "O(alive_bots * alive_players * body_probes)",
        "astrabot_perf_disable_vision",
        "astrabot_perf_disable_pathsearch",
        "astrabot_perf_disable_trace",
        "30 seconds",
        "P08 NOT STARTED",
    ):
        assert required in model, required


def test_p076_state_keeps_phase8_unstarted():
    state = (ROOT / ".planning" / "STATE.md").read_text(encoding="utf-8")
    roadmap = (ROOT / ".planning" / "ROADMAP.md").read_text(encoding="utf-8")
    assert "P07.6" in state
    assert "P08 NOT STARTED" in state
    assert "P07.6" in roadmap
    assert "P08 NOT STARTED" in roadmap


if __name__ == "__main__":
    test_p076_runtime_model_is_explicit()
    test_p076_state_keeps_phase8_unstarted()
    print("p07.6 docs: PASS")
