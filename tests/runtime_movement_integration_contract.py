"""Check orchestration and ownership of movement operations."""
from pathlib import Path

ADAPTER = Path(__file__).parents[1] / "src/adapter/metamod"

def source(name):
    return (ADAPTER / (name + ".cpp")).read_text(encoding="utf-8")

def test_start_frame_dispatches_nav_movement():
    plugin = source("plugin_runtime")
    start = plugin.index("void PluginRuntime::onStartFrame()")
    post = plugin.index("void PluginRuntime::onStartFramePost()", start)
    assert "updateManagedBotMovement" not in plugin[start:post]
    assert "updateManagedBotMovement" in plugin[post:]
    start = plugin.index("void PluginRuntime::updateManagedBotMovement()")
    end = plugin.index("void PluginRuntime::resetManagedBotTiming", start)
    body = plugin[start:end]
    phases = (
        "navigation_.observeFullUpdateMotion(",
        "behavior_.updateManagedBotCompatibilityState(",
        "navigation_.updateNavigation(",
        "behavior_.finishManagedBotNavigation(",
        "command_.buildAndExecuteMovement(",
    )
    positions = [body.index(phase) for phase in phases]
    assert positions == sorted(positions)
    assert "managedBotTiming_[index].advance(" in body
    assert "ActorState::Joined" in body
    assert "observation_.captureMovementPhysicsState(" in body
    assert "inputDispatcher_.dispatchNext" not in body
    assert "ActionAdapter::translate" not in body

    navigation = source("bot_navigation_runtime")
    command = source("bot_command_runtime")
    observation = source("bot_observation_runtime")
    behavior = source("bot_behavior_runtime")
    for field in ("v.mins.z", "NavRoamResult::MovementSuppressed", "managedBotMovement.update(",
                  "movementReadyLogged", "movement physics actor=%u",
                  "goalPresent=%d", "link=(%u->%u how=%u dir=%u)"):
        assert field in navigation, field
    for field in ("service_inputDispatcher().dispatchNext", "ActionAdapter::projectMovement",
                  "ActionAdapter::translate", "actionDispatch.stopMovement",
                  "pfnSetClientMaxspeed", "kDefaultManagedBotMaxSpeed",
                  "dispatchJoinHeartbeat", "dispatchNeutralMovement"):
        assert field in command, field
    assert "captureMovementPhysicsState" in observation
    assert "collectPlantedBomb" in behavior
    assert "carryingC4" in behavior

if __name__ == "__main__":
    test_start_frame_dispatches_nav_movement()
    print("runtime movement owner boundaries: PASS")
