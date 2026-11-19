"""Compare current callbacks with the frozen, pre-refactor implementation."""
import argparse
import difflib
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess

def apply_correction(baseline, fixture, platform):
    """Apply reviewed edits only at pinned offsets in a historical trace."""
    identity = fixture['platforms'][platform]
    if hashlib.sha256(baseline).hexdigest() != identity['baseline_sha256']:
        raise ValueError('historical baseline identity differs')
    records = baseline.decode().splitlines()
    output, cursor = [], 0
    for start, pattern_id in fixture['edits']:
        if not isinstance(start, int) or not isinstance(pattern_id, int) or pattern_id < 0:
            raise ValueError('invalid correction offset or pattern')
        if start < cursor or start > len(records):
            raise ValueError('overlapping or out-of-range correction')
        block = fixture['patterns'][pattern_id][platform]
        before, after = block['before'], block['after']
        if records[start:start + len(before)] != before:
            raise ValueError('correction context differs')
        if any(not isinstance(line, str) or '\n' in line or '\r' in line for line in after):
            raise ValueError('invalid inserted record')
        output.extend(records[cursor:start])
        output.extend(after)
        cursor = start + len(before)
    output.extend(records[cursor:])
    corrected = ('\n'.join(output) + '\n').encode() if output else b''
    if len(output) != identity['records'] or hashlib.sha256(corrected).hexdigest() != identity['corrected_sha256']:
        raise ValueError('reviewed correction identity differs')
    return corrected


def apply_corrections(baseline, corrections, platform):
    for correction in corrections:
        baseline = apply_correction(baseline, correction, platform)
    return baseline


def read_correction(path):
    data = path.read_bytes()
    return json.loads(gzip.decompress(data) if path.suffix == '.gz' else data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--correction', type=Path, action='append', default=[])
    parser.add_argument('--platform')
    args = parser.parse_args()
    args.output.unlink(missing_ok=True)
    env = dict(os.environ, ASTRABOT_CHARACTERIZATION_PATH=str(args.output.resolve()))
    result = subprocess.run([str(args.executable.resolve())], env=env, check=False)
    if result.returncode:
        raise SystemExit(result.returncode)
    expected = gzip.decompress(args.baseline.read_bytes())
    expected = apply_corrections(expected, [read_correction(path) for path in args.correction], args.platform)
    actual = args.output.read_bytes()
    if actual != expected:
        delta = difflib.unified_diff(expected.decode().splitlines(), actual.decode().splitlines(),
                                     fromfile='before-refactor', tofile='current', n=2)
        for _, line in zip(range(40), delta):
            print(line)
        raise SystemExit('runtime characterization differs; inspect output, do not regenerate baseline from candidate')
    label = 'match reviewed corrections; historical baseline retained' if args.correction else 'identical'
    print(f'runtime characterization: {len(actual.splitlines())} records {label}; SHA256={hashlib.sha256(actual).hexdigest()}')

if __name__ == '__main__':
    main()
