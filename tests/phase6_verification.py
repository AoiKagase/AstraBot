import argparse
import json
import sys
from pathlib import Path


PHASE6_CORE_DIRECTORIES = (
	Path("include/astrabot/nav"),
	Path("src/core/nav"),
)
REQUIRED_TARGETS = (
	"astrabot_nav_query",
	"astrabot_locomotion",
	"astrabot_jump_drop",
	"astrabot_special_traversal",
)
SDK_MARKERS = (
	"edict_t",
	"enginefuncs_t",
	"extdll.h",
	"RunPlayerMove",
	"metamod.h",
	"reapi",
)
DISPATCH_MARKERS = (
	"BotCommand",
	"CommandReceipt",
	"DispatchResult",
	"RunPlayerMove",
	"astrabot_mm",
)
NAV_WRITE_MARKERS = (
	"std::ofstream",
	"std::ios::out",
	"fopen",
	"fwrite",
	"WriteFile",
	"O_WRONLY",
	"O_RDWR",
)


def parse_arguments():
	default_root = Path(__file__).resolve().parents[1]
	parser = argparse.ArgumentParser(
		description="Verify the Phase 6 offline locomotion gate"
	)
	parser.add_argument("--root", type=Path, default=default_root)
	parser.add_argument(
		"--manifest",
		type=Path,
		default=default_root / "docs" / "source-manifest.json",
	)
	return parser.parse_args()


def check(condition, description, failures, checks):
	checks.append(description)
	if not condition:
		failures.append(description)


def load_manifest(path):
	try:
		return json.loads(path.read_text(encoding="utf-8"))
	except (OSError, UnicodeError, json.JSONDecodeError) as error:
		raise ValueError(f"cannot read manifest: {error}")


def phase6_core_paths(root):
	paths = []
	for directory in PHASE6_CORE_DIRECTORIES:
		absolute_directory = root / directory
		paths.extend(
			path.relative_to(root).as_posix()
			for path in absolute_directory.iterdir()
			if path.suffix in (".cpp", ".hpp")
		)
	return sorted(paths)


def read_text(path):
	return path.read_text(encoding="utf-8")


def verify(root, manifest_path):
	failures = []
	checks = []
	try:
		manifest = load_manifest(manifest_path)
	except ValueError as error:
		return [str(error)], checks

	manifest_paths = {
		entry.get("path")
		for entry in manifest.get("source_files", [])
		if isinstance(entry, dict)
	}
	core_paths = phase6_core_paths(root)
	check(
		all(path in manifest_paths for path in core_paths),
		"all Core navigation sources are manifest-registered",
		failures,
		checks,
	)

	core_text = "\n".join(read_text(root / path) for path in core_paths)
	check(
		all(marker not in core_text for marker in SDK_MARKERS),
		"Core navigation has no engine, Metamod, or ReAPI markers",
		failures,
		checks,
	)
	check(
		all(marker not in core_text for marker in NAV_WRITE_MARKERS),
		"Core navigation does not contain Nav write operations",
		failures,
		checks,
	)
	check(
		all(marker not in core_text for marker in DISPATCH_MARKERS),
		"route results are not named or mapped as dispatch success",
		failures,
		checks,
	)

	cmake_text = read_text(root / "CMakeLists.txt")
	check(
		all(target in cmake_text for target in REQUIRED_TARGETS),
		"all Phase 6 navigation test targets are present in CMake",
		failures,
		checks,
	)
	check(
		all(
			name in cmake_text
			for name in (
				"src/core/nav/locomotion.cpp",
				"src/core/nav/jump_drop.cpp",
				"src/core/nav/special_traversal.cpp",
				"tests/locomotion_tests.cpp",
				"tests/jump_drop_tests.cpp",
				"tests/special_traversal_tests.cpp",
			)
		),
		"Phase 6 sources and focused tests are registered in CMake",
		failures,
		checks,
	)

	requirements_text = read_text(root / ".planning" / "REQUIREMENTS.md")
	requirements_lower = requirements_text.lower()
	check(
		"[x] **par-01**" in requirements_lower and
		"live real-server" in requirements_lower and
		"remain pending in phase 8" in requirements_lower,
		"PAR-01 retains an explicit Phase 8 live-acceptance boundary",
		failures,
		checks,
	)
	roadmap_text = read_text(root / ".planning" / "ROADMAP.md")
	check(
		"[x] **Phase 6: Baseline locomotion**" in roadmap_text and
		"offline contracts completed" in roadmap_text and
		"| 6. Baseline locomotion | 5/5 | Complete (offline) |" in roadmap_text,
		"Phase 6 tracker records offline completion only",
		failures,
		checks,
	)
	check(
		"Phase 8: Differential and live parity acceptance" in roadmap_text,
		"Phase 8 remains represented as the live parity phase",
		failures,
		checks,
	)
	return failures, checks


def main():
	arguments = parse_arguments()
	root = arguments.root.resolve()
	manifest = arguments.manifest.resolve()
	try:
		failures, checks = verify(root, manifest)
	except (OSError, UnicodeError) as error:
		print(f"phase6 verification error: {error}", file=sys.stderr)
		return 1
	if failures:
		print("phase6 verification: FAILED", file=sys.stderr)
		for failure in failures:
			print(f"- {failure}", file=sys.stderr)
		return 1
	print(f"phase6 verification: OK ({len(checks)} checks)")
	return 0


if __name__ == "__main__":
	sys.exit(main())
