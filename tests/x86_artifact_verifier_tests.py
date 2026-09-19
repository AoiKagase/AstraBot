import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "verify_x86_artifact.py"
EXPORTS = [
	"Meta_Query",
	"Meta_Attach",
	"Meta_Detach",
	"GetEntityAPI2",
	"GetEntityAPI2_Post",
	"GetEngineFunctions",
	"GiveFnptrsToDll",
]


def current_artifact():
	if sys.platform == "win32":
		return ROOT / "build-metamod-x86-test" / "astrabot_mm.dll", "pe"
	return ROOT / "build-linux-x86-metamod-test" / "libastrabot_mm.so", "elf"


def run_verifier(path, artifact_format):
	command = [
		sys.executable,
		str(SCRIPT),
		"--path",
		str(path),
		"--format",
		artifact_format,
	]
	for export in EXPORTS:
		command.extend(["--export", export])
	return subprocess.run(command, capture_output=True, text=True)


class X86ArtifactVerifierTests(unittest.TestCase):
	def test_current_phase_artifact_is_x86_with_exact_exports(self):
		path, artifact_format = current_artifact()
		result = run_verifier(path, artifact_format)
		self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

	def test_missing_artifact_is_rejected(self):
		path, artifact_format = current_artifact()
		result = run_verifier(path.with_name(path.name + ".missing"), artifact_format)
		self.assertNotEqual(result.returncode, 0)

	def test_wrong_format_fixture_is_rejected(self):
		with tempfile.TemporaryDirectory() as directory:
			path = Path(directory) / "not-an-artifact.bin"
			path.write_bytes(b"not an executable")
			result = run_verifier(path, "pe")

		self.assertNotEqual(result.returncode, 0)
		self.assertIn("format", result.stdout.lower() + result.stderr.lower())


if __name__ == "__main__":
	unittest.main()
