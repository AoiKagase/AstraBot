import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "source-manifest.json"
SCRIPT = ROOT / "tools" / "check_source_manifest.py"


def run_checker(manifest_path):
	return subprocess.run(
		[
			sys.executable,
			str(SCRIPT),
			"--root",
			str(ROOT),
			"--manifest",
			str(manifest_path),
		],
		capture_output=True,
		text=True,
	)


class SourceManifestCheckerTests(unittest.TestCase):
	def test_valid_manifest_is_accepted(self):
		result = run_checker(MANIFEST)
		self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

	def test_missing_source_entry_is_rejected(self):
		data = json.loads(MANIFEST.read_text(encoding="utf-8"))
		data["source_files"] = [
			entry
			for entry in data["source_files"]
			if entry["path"] != "src/core/build_identity.cpp"
		]
		with tempfile.TemporaryDirectory() as directory:
			manifest_path = Path(directory) / "manifest.json"
			manifest_path.write_text(json.dumps(data), encoding="utf-8")
			result = run_checker(manifest_path)

		self.assertNotEqual(result.returncode, 0)
		self.assertIn("source", result.stdout.lower() + result.stderr.lower())

	def test_wrong_reference_commit_is_rejected(self):
		data = json.loads(MANIFEST.read_text(encoding="utf-8"))
		data["references"][0]["commit"] = "0" * 40
		with tempfile.TemporaryDirectory() as directory:
			manifest_path = Path(directory) / "manifest.json"
			manifest_path.write_text(json.dumps(data), encoding="utf-8")
			result = run_checker(manifest_path)

		self.assertNotEqual(result.returncode, 0)
		self.assertIn("commit", result.stdout.lower() + result.stderr.lower())


if __name__ == "__main__":
	unittest.main()
