"""Executable guard for the private runtime dependency direction."""
from pathlib import Path
import re
from runtime_architecture import check_dependencies, lex

ROOT = Path(__file__).resolve().parents[1]
ADAPTER = ROOT / "src/adapter/metamod"


def main():
    ports = ' '.join(token.value if token.kind == 'code' else '<literal>' for token in
                     lex((ADAPTER / "runtime/shared/bot_runtime_services.hpp").read_text(encoding="utf-8")))
    assert "class BotRuntimeServices" not in ports, "all-owner service locator remains"
    for owner in ("Observation", "Behavior", "Navigation", "Command"):
        assert "class Bot" + owner + "Services" in ports
    nav = (ADAPTER / "runtime/navigation/bot_navigation_runtime.cpp").read_text(encoding="utf-8")
    assert "MovementPreparation" in nav
    # Include every split implementation and public Core header, not just four
    # original entrypoint files. Comment/string contents are not executable code.
    for base in (ROOT / 'src', ROOT / 'include/astrabot'):
        for path in sorted(base.rglob('*')):
            if path.suffix in {'.cpp', '.hpp', '.h'}:
                check_dependencies(path.relative_to(ROOT).as_posix(), path.read_text(encoding='utf-8'))
    print("runtime ports and orchestration boundary: PASS")


if __name__ == "__main__":
    main()
