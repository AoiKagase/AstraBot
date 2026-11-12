"""Compare current callbacks with the frozen, pre-refactor implementation."""
import argparse
import difflib
import gzip
import hashlib
import os
from pathlib import Path
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.unlink(missing_ok=True)
    env = dict(os.environ, ASTRABOT_CHARACTERIZATION_PATH=str(args.output.resolve()))
    result = subprocess.run([str(args.executable.resolve())], env=env, check=False)
    if result.returncode:
        raise SystemExit(result.returncode)
    expected = gzip.decompress(args.baseline.read_bytes())
    actual = args.output.read_bytes()
    if actual != expected:
        delta = difflib.unified_diff(expected.decode().splitlines(), actual.decode().splitlines(),
                                     fromfile='before-refactor', tofile='current', n=2)
        for _, line in zip(range(40), delta):
            print(line)
        raise SystemExit('runtime characterization differs; inspect output, do not regenerate baseline from candidate')
    print(f'runtime characterization: {len(actual.splitlines())} records identical; SHA256={hashlib.sha256(actual).hexdigest()}')

if __name__ == '__main__':
    main()
