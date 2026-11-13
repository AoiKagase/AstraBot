from source_layout_contract import adapter_path
"""Check the ownership boundary promised by the responsibility cleanup."""
from pathlib import Path
import re
import json
from runtime_architecture import collect_members, check_inventory

ROOT = Path(__file__).resolve().parents[1]
OWNERS = {
    'bot_observation_runtime': 'BotObservationRuntime',
    'bot_behavior_runtime': 'BotBehaviorRuntime',
    'bot_navigation_runtime': 'BotNavigationRuntime',
    'bot_command_runtime': 'BotCommandRuntime',
}

def main():
    nav_header = (ROOT / 'include/astrabot/runtime/nav_roam_controller.hpp').read_text(encoding='utf-8')
    for group in ('RoamGoalState', 'RouteState', 'TraversalState', 'FriendWaitState', 'RecoveryState'):
        assert 'struct ' + group in nav_header, f'missing NAV state boundary: {group}'
    inventory = json.loads((ROOT / 'tests/fixtures/runtime_ownership.json').read_text(encoding='utf-8'))
    declarations = {}
    owners = dict(OWNERS, plugin_runtime="PluginRuntime")
    for stem, owner in owners.items():
        path = adapter_path(stem + ".hpp").relative_to(ROOT).as_posix()
        declarations[path] = collect_members((ROOT / path).read_text(encoding="utf-8"), owner)
    expected = {(entry["header"], entry["scope"], entry["member"]) for entry in inventory}
    assert len(expected) == len(inventory), "duplicate ownership fixture entry"
    check_inventory(declarations, expected)
    print('runtime ownership and complete source inventory: PASS')

if __name__ == '__main__':
    main()
