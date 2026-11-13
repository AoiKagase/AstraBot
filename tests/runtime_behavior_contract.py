"""Keep behavior entrypoints as ordered orchestration, not mixed implementations."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADAPTER = ROOT / "src/adapter/metamod"


def body(text, signature):
    start = text.index("{", text.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    source = (ADAPTER / "bot_behavior_runtime.cpp").read_text(encoding="utf-8")
    action = body(source, "BotBehaviorRuntime::decideManagedBotAction(")
    state = body(source, "BotBehaviorRuntime::updateManagedBotCompatibilityState(")
    assert len(action.splitlines()) < 60, "action entrypoint still mixes implementations"
    assert len(state.splitlines()) < 60, "state entrypoint still mixes implementations"
    action_phases = ("prepareManagedAction(", "decideObjectiveAction(", "decideCombatAction(")
    state_phases = ("updateBehaviorLifecycle(", "updateScenarioState(", "updateBehaviorPriorities(", "commitBehaviorState(")
    for text, phases in ((action, action_phases), (state, state_phases)):
        positions = [text.index(phase) for phase in phases]
        assert positions == sorted(positions), "behavior phase order changed"
    for unit in ("buying", "radio", "objectives", "combat", "scenario", "perception", "priorities"):
        assert (ADAPTER / ("bot_behavior_" + unit + ".cpp")).is_file(), unit
    print("behavior responsibility and phase boundaries: PASS")


if __name__ == "__main__":
    main()
