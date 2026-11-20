"""Keep navigation orchestration separate from its ordered update phases."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def main():
    source = (ROOT / "src/core/runtime/nav_roam_controller.cpp").read_text(encoding="utf-8")
    start = source.index("NavRoamResult NavRoamController::updateInternal(")
    end = source.index("\nvoid NavRoamController::reset()", start)
    orchestrator = source[start:source.index("\n}", start) + 2]
    assert len(orchestrator.splitlines()) < 100, "navigation update still mixes responsibilities"
    phases = ("validateNavigationUpdate", "prepareNoiseRoute", "updateFriendWaiting", "locateUpdateAreas",
              "recoverOffMeshUpdate", "recoverStuckUpdate", "selectUpdateRoute",
              "startUpdateTraversal", "updateJumpDropTraversal", "updateSpecialTraversal",
              "updateGroundLocomotion")
    positions = [orchestrator.index(name + "(") for name in phases]
    assert positions == sorted(positions), "navigation phase order changed"
    for name in phases:
        assert "NavRoamController::" + name + "(" in source[start:end], name
    public_start = source.rindex("NavRoamResult NavRoamController::update(", 0, start)
    public = source[public_start:source.index("\n}", public_start) + 2]
    public_phases = ("updateInternal(", "resetRoute(true)", "lastWiggleJumpAt", "replaceFollowRoute(", "return result;")
    positions = [public.index(phase) for phase in public_phases]
    assert positions == sorted(positions), "Follow route replacement must run after movement and its cleanup"
    assert re.search(r"struct NavigationUpdateContext", (ROOT / "include/astrabot/runtime/nav_roam_controller.hpp").read_text(encoding="utf-8"))
    print("navigation responsibility and phase boundaries: PASS")


if __name__ == "__main__":
    main()
