"""Check the ownership boundary promised by the responsibility cleanup."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OWNERS = {
    'bot_observation_runtime': 'BotObservationRuntime',
    'bot_behavior_runtime': 'BotBehaviorRuntime',
    'bot_navigation_runtime': 'BotNavigationRuntime',
    'bot_command_runtime': 'BotCommandRuntime',
}

def main():
    adapter = ROOT / 'src/adapter/metamod'
    for stem, name in OWNERS.items():
        header = adapter / (stem + '.hpp')
        assert header.exists(), f'missing responsible owner: {name}'
        text = header.read_text(encoding='utf-8')
        assert re.search(r'class\s+' + name + r'\b', text), name
        assert 'struct ActorState' in text, f'{name} needs actor-local state'
        assert 'friend class' not in text, f'{name} must not expose ownership through friends'
        assert re.search(r'private:\s*\n[\s\S]*struct ActorState', text), f'{name} actor state must be private'
        assert 'ActorState &' not in text.split('private:')[0], f'{name} exposes writable actor state'
        assert 'const_cast' not in text, f'{name} bypasses ownership'
    nav_header = (ROOT / 'include/astrabot/runtime/nav_roam_controller.hpp').read_text(encoding='utf-8')
    for group in ('RoamGoalState', 'RouteState', 'TraversalState', 'FriendWaitState', 'RecoveryState'):
        assert 'struct ' + group in nav_header, f'missing NAV state boundary: {group}'
    document = (ROOT / 'docs/RESPONSIBILITIES.md').read_text(encoding='utf-8')
    # The inventory must agree with declaration locations and storage scope,
    # including shared members which never belonged to ActorState.
    state_rows = 0
    for row in document.splitlines():
        cells = [cell.strip() for cell in row.split('|')[1:-1]]
        if not cells or not re.fullmatch(r'`\w+_`', cells[0]):
            continue
        state_rows += 1
        name = cells[0].strip('`')
        location = re.fullmatch(r'`([^`]+\.hpp):(\d+)`', cells[8])
        assert location, f'{name}: missing declaration location'
        header = (ROOT / location[1]).read_text(encoding='utf-8')
        lines = header.splitlines()
        line_number = int(location[2])
        declaration = lines[line_number - 1]
        actor = re.search(r'struct ActorState\s*\{', header)
        actor_end = -1
        if actor:
            depth = 1
            for offset in range(actor.end(), len(header)):
                depth += (header[offset] == '{') - (header[offset] == '}')
                if depth == 0:
                    actor_end = offset
                    break
        declaration_offset = sum(len(line) + 1 for line in lines[:line_number - 1])
        actor_local = actor is not None and actor.start() <= declaration_offset < actor_end
        member = name[:-1] if actor_local else name
        assert re.search(r'\b' + re.escape(member) + r'\b', declaration), f'{name}: wrong anchor'
        prefix = declaration.split(member, 1)[0]
        assert not any(token in prefix for token in ('#include', 'return ', '(', ')')), f'{name}: anchor is a reference'
        assert ('private ActorState' in cells[2]) == actor_local, f'{name}: incorrect actor/shared scope'
    assert state_rows >= 200, 'incomplete mutable-state inventory'
    assert '切断時は武器キャッシュとlookaheadの保存値を保持' in document, 'disconnect retention must be explicit'
    for path in sorted((ROOT / 'src').rglob('*')) + sorted((ROOT / 'include/astrabot').rglob('*')):
        if path.suffix in {'.cpp', '.hpp', '.h', '.c', '.def', '.map'}:
            rel = path.relative_to(ROOT).as_posix()
            assert rel in document, f'unassigned source: {rel}'
    print('runtime ownership and complete source inventory: PASS')

if __name__ == '__main__':
    main()
