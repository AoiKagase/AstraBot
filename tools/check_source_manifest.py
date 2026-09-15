#!/usr/bin/env python3

import argparse
import json
import sys
from pathlib import Path


CPP_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp"}
EXPECTED_REFERENCES = {
    "ReGameDLL-CS": {
        "path": "../ReGameDLL_CS",
        "commit": "b0889847fe6d03898be88acc9e366660efb40ab5",
    },
    "Metamod-P": {
        "path": "../metamod-p",
        "commit": "7ec9b014f8c0a947a724644aebe34eb33706e44b",
    },
}
SOURCE_DIRECTORIES = ("src", "include", "tests", "tools")


def fail(message):
    print(f"source manifest error: {message}", file=sys.stderr)
    return 1


def normalize_path(value):
    return value.replace("\\", "/")


def is_safe_relative_path(value):
    normalized = normalize_path(value)
    return normalized and not Path(normalized).is_absolute() and normalized not in {".", ".."} and not normalized.startswith("../")


def resolve_project_path(root, value):
    normalized = normalize_path(value)
    path = (root / normalized).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError:
        return None
    return path


def load_manifest(path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise ValueError(f"manifest does not exist: {path}")
    except json.JSONDecodeError as error:
        raise ValueError(f"manifest is not valid JSON: {error}")
    except UnicodeDecodeError as error:
        raise ValueError(f"manifest is not UTF-8: {error}")


def collect_project_sources(root):
    paths = set()
    for directory in SOURCE_DIRECTORIES:
        base = root / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in CPP_EXTENSIONS:
                paths.add(path.relative_to(root).as_posix())
    return paths


def check_manifest(root, manifest_path):
    try:
        data = load_manifest(manifest_path)
    except ValueError as error:
        return fail(str(error))

    if not isinstance(data, dict):
        return fail("top-level value must be an object")
    if data.get("schema_version") != 1:
        return fail("schema_version must be 1")
    if data.get("project") != "AstraBot":
        return fail("project must be AstraBot")
    if not data.get("release_license_status"):
        return fail("release_license_status must be present")

    errors = []
    references = data.get("references")
    if not isinstance(references, list):
        errors.append("references must be a list")
        references = []
    reference_names = set()
    for reference in references:
        if not isinstance(reference, dict):
            errors.append("each reference must be an object")
            continue
        name = reference.get("name")
        reference_names.add(name)
        expected_reference = EXPECTED_REFERENCES.get(name)
        if expected_reference is None:
            errors.append(f"unapproved reference: {name}")
        elif reference.get("commit") != expected_reference["commit"]:
            errors.append(f"reference {name} has an unexpected commit")
        if expected_reference is not None and reference.get("path") != expected_reference["path"]:
            errors.append(f"reference {name} has an unexpected path")
        if reference.get("mutable") is not False:
            errors.append(f"reference {name} must be immutable")
        reference_path = (root / normalize_path(reference.get("path", ""))).resolve()
        if reference_path is None or not reference_path.is_dir():
            errors.append(f"reference {name} path does not resolve to a directory")
    missing_references = set(EXPECTED_REFERENCES) - reference_names
    errors.extend(f"missing reference: {name}" for name in sorted(missing_references))

    project_files = data.get("project_files")
    if not isinstance(project_files, list):
        errors.append("project_files must be a list")
        project_files = []
    for value in project_files:
        if not isinstance(value, str) or not is_safe_relative_path(value):
            errors.append(f"unsafe project file path: {value}")
            continue
        path = resolve_project_path(root, value)
        if path is None or not path.is_file():
            errors.append(f"missing project file: {value}")

    source_files = data.get("source_files")
    if not isinstance(source_files, list):
        return fail("source_files must be a list")

    listed_paths = []
    for entry in source_files:
        if not isinstance(entry, dict):
            errors.append("each source_files entry must be an object")
            continue
        value = entry.get("path")
        if not isinstance(value, str) or not is_safe_relative_path(value):
            errors.append(f"unsafe source file path: {value}")
            continue
        normalized = normalize_path(value)
        listed_paths.append(normalized)
        path = resolve_project_path(root, normalized)
        if path is None or not path.is_file():
            errors.append(f"missing source file: {normalized}")
        if entry.get("origin") != "AstraBot":
            errors.append(f"source file has an unapproved origin: {normalized}")
        if entry.get("reference_only") is not False:
            errors.append(f"project source cannot be reference-only: {normalized}")
        if not entry.get("license_status"):
            errors.append(f"source file has no license status: {normalized}")

    duplicates = sorted({path for path in listed_paths if listed_paths.count(path) > 1})
    errors.extend(f"duplicate source file entry: {path}" for path in duplicates)

    listed_cpp_paths = {path for path in listed_paths if Path(path).suffix.lower() in CPP_EXTENSIONS}
    actual_cpp_paths = collect_project_sources(root)
    errors.extend(f"unlisted project source: {path}" for path in sorted(actual_cpp_paths - listed_cpp_paths))
    errors.extend(f"manifest source does not exist in source roots: {path}" for path in sorted(listed_cpp_paths - actual_cpp_paths))

    if errors:
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(f"source manifest: OK ({len(source_files)} entries, {len(actual_cpp_paths)} C/C++ files)")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Validate AstraBot source provenance")
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()
    return check_manifest(args.root.resolve(), args.manifest.resolve())


if __name__ == "__main__":
    sys.exit(main())
