# SPDX-License-Identifier: MPL-2.0
"""Exercise fail-closed evidence validation with independently authored results."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
import sys

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("checker", ROOT / "tools/check-movement-evidence.py")
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class EvidenceContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "fixture.cpp").write_bytes(b"independent fixture\n")
        self.manifest = {"schemaVersion": 1, "seed": 308, "producers": {
            "portable": {"fixtureFiles": ["fixture.cpp"], "cases": [{
                "scenario": "floor", "frameUs": [8000], "actors": [1],
                "variants": {"clean": "Arrived"}, "maxFrames": 100,
                "maxQueriesPerActorFrame": 21, "maxReplans": 0, "traceLimit": 1000,
                "terminalOutcomes":{"clean":{"default":"Arrived"}}}]}}}
        self.evidence = {"schemaVersion": 1, "producer": "portable", "context": {
            "revision": "a" * 40, "dirty": False, "diffSha256": "b" * 64,
            "manifestSha256": checker.digest(self.manifest),
            "fixtureHashes": {"fixture.cpp": hashlib.sha256(b"independent fixture\n").hexdigest()},
            "platform": "test", "architecture": "x86", "buildOptions": {"CMAKE_BUILD_TYPE": "Debug"},
            "executableSha256": "c" * 64, "compiler": "test compiler", "hostedRun": None},
            "results": [{"scenario": "floor", "frameUs": 8000, "actors": 1,
                "variant": "clean", "seed": 308, "outcome": "Arrived", "expectedOutcome": "Arrived",
                "frames": 2, "elapsedUs": 16000, "maxQueriesPerActorFrame": 5,
                "totalQueries": 10, "replans": 0, "traceLimit": 1000,
                "trace": [{"type":"input","actor":1,"tick":1,"map":1,"start":[50,50,36],"goalArea":2},
                          {"type":"command","actor":1,"tick":1,"map":1,"movement":[100,0,0],"view":[0,0,0],"buttons":0,"msec":8},
                          {"type":"terminal","actor":1,"tick":2,"map":1,"outcome":"Arrived"}],
                "traceTruncated": False, "replayEqual": True}]}

    def test_accepts_independently_authored_complete_evidence(self):
        self.assertEqual(checker.validate(self.manifest, self.evidence, self.root), 1)

    def reject(self, mutate):
        evidence = copy.deepcopy(self.evidence)
        mutate(evidence)
        with self.assertRaises(ValueError):
            checker.validate(self.manifest, evidence, self.root)

    def test_rejects_missing_duplicate_and_extra_cases(self):
        self.reject(lambda e: e.update(results=[]))
        self.reject(lambda e: e["results"].append(copy.deepcopy(e["results"][0])))
        self.reject(lambda e: e["results"][0].update(scenario="invented"))

    def test_rejects_false_success_and_unbounded_results(self):
        for change in ({"outcome": "Failed"}, {"expectedOutcome": "Failed"},
                       {"traceTruncated": True}, {"replayEqual": False}, {"trace": []},
                       {"frames": 101}, {"elapsedUs": 1}, {"maxQueriesPerActorFrame": 22},
                       {"totalQueries": 43}, {"replans": 1}, {"seed": 309},
                       {"traceLimit": 1001}, {"frames": True}, {"actors": True},
                       {"totalQueries": -1}, {"maxQueriesPerActorFrame": float("nan")}):
            with self.subTest(change=change):
                self.reject(lambda e: e["results"][0].update(change))

    def test_rejects_schema_and_provenance_mismatches(self):
        self.reject(lambda e: e.update(schemaVersion=2))
        self.reject(lambda e: e.update(producer="unknown"))
        self.reject(lambda e: e["context"].update(architecture="x64"))
        self.reject(lambda e: e["context"].update(manifestSha256="0" * 64))
        self.reject(lambda e: e["context"]["fixtureHashes"].update({"fixture.cpp": "0" * 64}))
        self.reject(lambda e: e.pop("context"))
        (self.root / "fixture.cpp").write_bytes(b"changed fixture")
        with self.assertRaises(ValueError):
            checker.validate(self.manifest, self.evidence, self.root)

    def test_rejects_invalid_manifest(self):
        for field, value in (("maxFrames", 0), ("frameUs", [8000, 8000]), ("actors", [0])):
            manifest = copy.deepcopy(self.manifest)
            manifest["producers"]["portable"]["cases"][0][field] = value
            with self.assertRaises(ValueError):
                checker.validate(manifest, self.evidence, self.root)

    def test_rejects_malformed_trace_and_missing_actor_terminal(self):
        for trace in ([None], [{"type":"invented","actor":1,"tick":1}],
                      [{"type":"terminal","actor":2,"tick":2,"outcome":"Arrived"}],
                      [{"type":"terminal","actor":1,"tick":0,"outcome":"Arrived"}],
                      [{"type":"terminal","actor":1,"tick":2,"outcome":"MadeUp"}],
                      [{"type":"terminal","actor":1,"tick":2,"outcome":"Failed"}],
                      self.evidence["results"][0]["trace"][:1]):
            with self.subTest(trace=trace):
                self.reject(lambda e: e["results"][0].update(trace=trace))

    def test_rejects_evidence_from_different_source_or_build(self):
        for key in ("revision", "diffSha256", "executableSha256", "buildOptions"):
            expected = copy.deepcopy(self.evidence["context"])
            expected[key] = "different"
            with self.subTest(key=key), self.assertRaises(ValueError):
                checker.validate(self.manifest, self.evidence, self.root, expected)


if __name__ == "__main__":
    unittest.main()
