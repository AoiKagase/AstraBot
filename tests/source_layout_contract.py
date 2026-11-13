"""Keep implementation layout and local artifacts separate from public contracts."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ADAPTER = ROOT / 'src/adapter/metamod'


def adapter_path(name):
    matches = sorted(ADAPTER.rglob(name))
    assert len(matches) == 1, f'expected one adapter file for {name}: {matches}'
    return matches[0]


def main():
    for directory in ('plugin', 'runtime/behavior', 'runtime/observation',
                      'runtime/navigation', 'runtime/command', 'runtime/shared',
                      'bridge', 'lifecycle', 'config', 'navigation', 'diagnostics'):
        assert (ADAPTER / directory).is_dir(), f'missing responsibility directory: {directory}'
    assert not list(ADAPTER.glob('*.cpp')), 'adapter implementation remains flat'
    assert not list(ADAPTER.glob('*.hpp')), 'private adapter headers remain flat'
    names = [path.name for path in ADAPTER.rglob('*') if path.is_file()]
    assert len(names) == len(set(names)), 'ambiguous duplicate private adapter filename'
    for base in (ROOT / 'src', ROOT / 'include'):
        for path in base.rglob('*'):
            assert path.suffix not in ('.obj', '.dll', '.zip', '.bak', '.orig'), path
    for path in (ROOT / 'tests').rglob('*.cpp'):
        for reference in re.findall(r'"(src/adapter/metamod/[^"\n]+\.(?:cpp|hpp))"',
                                    path.read_text(encoding='utf-8')):
            assert (ROOT / reference).is_file(), f'{path.name} reads a missing source: {reference}'
    print('source responsibility layout: PASS')


if __name__ == '__main__':
    main()
