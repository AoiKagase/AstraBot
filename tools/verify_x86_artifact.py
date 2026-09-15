#!/usr/bin/env python3

import argparse
import re
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path


PE_EXPORT_RE = re.compile(r"^\s*\d+\s+\d+\s+[0-9A-Fa-f]+\s+([^\s=]+)")


def fail(message):
    print(f"artifact verification error: {message}", file=sys.stderr)
    return 1


def run_tool(command):
    try:
        result = subprocess.run(command, capture_output=True, text=True)
    except OSError as error:
        return None, str(error)
    output = f"{result.stdout}\n{result.stderr}".strip()
    return result, output


def verify_magic(path, artifact_format):
    try:
        with path.open("rb") as stream:
            magic = stream.read(4)
    except OSError as error:
        return fail(f"artifact cannot be read: {error}")

    if artifact_format == "pe" and magic[:2] != b"MZ":
        return fail("format: expected a PE file with an MZ header")
    if artifact_format == "elf" and magic != b"\x7fELF":
        return fail("format: expected an ELF file")
    return 0


def verify_pe(path, expected):
    dumpbin = shutil.which("dumpbin")
    if dumpbin is None:
        return fail("dumpbin is not available in PATH")

    headers, header_output = run_tool([dumpbin, "/headers", str(path)])
    if headers is None:
        return fail(f"dumpbin headers failed: {header_output}")
    if headers.returncode != 0:
        return fail(f"dumpbin headers failed: {header_output}")
    if "(x86)" not in header_output:
        return fail("architecture: PE artifact is not marked as x86")

    exports, export_output = run_tool([dumpbin, "/exports", str(path)])
    if exports is None:
        return fail(f"dumpbin exports failed: {export_output}")
    if exports.returncode != 0:
        return fail(f"dumpbin exports failed: {export_output}")
    actual = [match.group(1) for line in export_output.splitlines() if (match := PE_EXPORT_RE.match(line))]
    return verify_export_set(actual, expected, "PE")


def read_elf_exports(path):
    nm = shutil.which("nm")
    if nm is not None:
        result, output = run_tool([nm, "-D", "--defined-only", str(path)])
        if result is not None and result.returncode == 0:
            return [line.split()[-1] for line in output.splitlines() if len(line.split()) >= 3], None

    readelf = shutil.which("readelf")
    if readelf is None:
        return None, "neither nm nor readelf is available in PATH"
    result, output = run_tool([readelf, "-Ws", str(path)])
    if result is None or result.returncode != 0:
        return None, output
    names = []
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[0].rstrip(":").isdigit():
            names.append(fields[-1].split("@", 1)[0])
    return names, None


def verify_elf(path, expected):
    readelf = shutil.which("readelf")
    if readelf is None:
        return fail("readelf is not available in PATH")
    headers, header_output = run_tool([readelf, "-h", str(path)])
    if headers is None or headers.returncode != 0:
        return fail(f"readelf headers failed: {header_output}")
    if not re.search(r"^\s*Class:\s+ELF32\s*$", header_output, re.MULTILINE):
        return fail("architecture: ELF artifact is not ELF32")
    if not re.search(r"^\s*Machine:\s+Intel 80386\s*$", header_output, re.MULTILINE):
        return fail("architecture: ELF artifact is not Intel 80386")

    actual, error = read_elf_exports(path)
    if actual is None:
        return fail(f"ELF export inspection failed: {error}")
    return verify_export_set(actual, expected, "ELF")


def verify_export_set(actual, expected, label):
    expected_set = set(expected)
    actual_counter = Counter(actual)
    actual_set = set(actual)
    duplicates = sorted(name for name, count in actual_counter.items() if count > 1 and name in expected_set)
    missing = sorted(expected_set - actual_set)
    extra = sorted(actual_set - expected_set)
    if duplicates or missing or extra:
        if duplicates:
            print(f"- duplicate {label} exports: {', '.join(duplicates)}", file=sys.stderr)
        if missing:
            print(f"- missing {label} exports: {', '.join(missing)}", file=sys.stderr)
        if extra:
            print(f"- extra {label} exports: {', '.join(extra)}", file=sys.stderr)
        return 1
    print(f"artifact: OK ({label} x86, {len(expected_set)} exact exports)")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Verify an x86 PE or ELF artifact")
    parser.add_argument("--path", required=True, type=Path)
    parser.add_argument("--format", required=True, choices=("pe", "elf"))
    parser.add_argument("--export", action="append", required=True)
    args = parser.parse_args()

    path = args.path.resolve()
    if not path.is_file():
        return fail(f"artifact does not exist: {path}")
    magic_result = verify_magic(path, args.format)
    if magic_result != 0:
        return magic_result
    if args.format == "pe":
        return verify_pe(path, args.export)
    return verify_elf(path, args.export)


if __name__ == "__main__":
    sys.exit(main())
