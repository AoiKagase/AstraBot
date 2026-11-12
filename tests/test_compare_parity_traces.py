"""Synthetic comparator tests; these fixtures are not native CSBot captures."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("compare_parity_traces", Path(__file__).resolve().parents[1] / "tools" / "compare_parity_traces.py")
comparator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(comparator)


class TraceComparisonTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.reference = Path(self.directory.name) / "reference.jsonl"
        self.candidate = Path(self.directory.name) / "candidate.jsonl"
        self.metadata = {"kind": "metadata", "schema": comparator.SCHEMA, "capture_kind": "fixture",
                         "reference_pin": "b0889847fe6d03898be88acc9e366660efb40ab5", "map": "de_fixture",
                         "scenario": "synthetic comparator test", "profiles": {"1:1": "fixture"},
                         "cvars": {"bot_difficulty": 2}, "rng_seed": 7,
                         "identity_settings": {"actors": "slot:generation"}, "coverage": ["command"]}

    def event(self, name="a", sequence=1):
        return {"id": name, "kind": "command", "sequence": sequence, "timestamp": 1.0,
                "actor": "1:1", "frame": {"map_generation": 1, "round_generation": 1, "tick": sequence},
                "data": {"forwardmove": 0.0, "sidemove": 0.0, "upmove": 0.0, "buttons": 1,
                         "view": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0}, "msec": 33, "action": None}}

    def write(self, path, events, metadata=None):
        path.write_text("".join(json.dumps(row) + "\n" for row in [metadata or self.metadata] + events), encoding="utf-8")

    def compare(self, left, right, precision=None, reasons=None, candidate_metadata=None):
        self.write(self.reference, left)
        self.write(self.candidate, right, candidate_metadata)
        return comparator.compare_traces(self.reference, self.candidate, precision, reasons)

    def test_fixture_match_is_not_live_acceptance(self):
        report = self.compare([self.event()], [self.event()])
        self.assertTrue(report["match"])
        self.assertFalse(report["live_parity_accepted"])
        self.assertFalse(report["complete_record_classes"])

    def test_missing_and_extra(self):
        report = self.compare([self.event("a"), self.event("b", 2)], [self.event("a"), self.event("c", 2)])
        self.assertIn({"type": "missing", "event": "b"}, report["findings"])
        self.assertIn({"type": "extra", "event": "c"}, report["findings"])

    def test_reorder_even_when_sequence_numbers_are_renumbered(self):
        report = self.compare([self.event("a"), self.event("b", 2)], [self.event("b"), self.event("a", 2)])
        self.assertTrue(any(finding["type"] == "reorder" for finding in report["findings"]))

    def test_configuration_and_identity_are_exact(self):
        for field in ("profiles", "cvars", "identity_settings", "rng_seed", "reference_pin"):
            with self.subTest(field=field):
                metadata = copy.deepcopy(self.metadata)
                metadata[field] = ("f" * 40 if field == "reference_pin" else 8 if field == "rng_seed" else {"changed": True})
                self.assertFalse(self.compare([self.event()], [self.event()], candidate_metadata=metadata)["match"])

    def test_all_record_classes_compare_payload_and_order(self):
        events = []
        for sequence, (kind, fields) in enumerate(comparator.FIELDS.items(), 1):
            event = self.event(kind, sequence)
            event["kind"] = kind
            # Synthetic values test traversal, not a measured behavioral recording.
            event["data"] = {field: {"ordered": [1, 2], "known": True} for field in fields}
            events.append(event)
        self.metadata["coverage"] = list(comparator.FIELDS)
        self.assertTrue(self.compare(events, copy.deepcopy(events))["complete_record_classes"])
        for index in range(len(events)):
            with self.subTest(kind=events[index]["kind"]):
                changed = copy.deepcopy(events)
                field = sorted(changed[index]["data"])[0]
                changed[index]["data"][field]["ordered"] = [2, 1]
                self.assertFalse(self.compare(events, changed)["match"])

    def test_precision_defaults_to_zero_and_requires_evidence(self):
        changed = self.event()
        changed["data"]["view"]["yaw"] = 0.001
        self.assertFalse(self.compare([self.event()], [changed])["match"])
        with self.assertRaises(comparator.TraceError):
            self.compare([self.event()], [changed], {"/data/view/yaw": 0.002})
        report = self.compare([self.event()], [changed], {"/data/view/yaw": 0.002},
                              {"/data/view/yaw": "synthetic test of an explicit absolute bound"})
        self.assertTrue(report["match"])

    def test_timestamp_precision_does_not_relax_buttons_or_msec(self):
        changed = self.event()
        changed["timestamp"] += 0.001
        self.assertTrue(self.compare([self.event()], [changed], {"/timestamp": 0.002}, {"/timestamp": "test bound"})["match"])
        changed["data"]["buttons"] = 2
        self.assertFalse(self.compare([self.event()], [changed], {"/timestamp": 0.002}, {"/timestamp": "test bound"})["match"])
        with self.assertRaises(comparator.TraceError):
            self.compare([self.event()], [changed], {"/data/buttons": 1.0}, {"/data/buttons": "invalid relaxation"})

    def test_precision_rejects_rng_identity_and_nonfinite_bounds(self):
        for path, epsilon in (("/data/value", 1.0), ("/frame/tick", 1.0),
                              ("/sequence", 1.0), ("/timestamp", float("inf")),
                              ("/timestamp", -1.0), ("/timestamp", 0.0)):
            with self.subTest(path=path, epsilon=epsilon), self.assertRaises(comparator.TraceError):
                comparator.validate_precision({path: epsilon}, {path: "synthetic rejected override"})

    def test_invalid_or_ambiguous_inputs_are_rejected(self):
        for mutate in (lambda event: event.update(sequence=True),
                       lambda event: event.update(timestamp=float("nan")),
                       lambda event: event["data"].pop("buttons")):
            with self.subTest(mutate=mutate):
                invalid = self.event()
                mutate(invalid)
                with self.assertRaises(comparator.TraceError):
                    self.compare([self.event()], [invalid])
        with self.assertRaises(comparator.TraceError):
            self.compare([self.event()], [self.event(), self.event()])

    def test_duplicate_keys_and_unhashable_kind_are_input_errors(self):
        self.write(self.reference, [self.event()])
        self.candidate.write_text('{"kind":"metadata","kind":"metadata"}\n', encoding="utf-8")
        with self.assertRaises(comparator.TraceError):
            comparator.load_trace(self.candidate)
        changed = self.event()
        changed["kind"] = []
        with self.assertRaises(comparator.TraceError):
            self.compare([self.event()], [changed])

    def test_cli_exit_codes_and_empty_trace_rejection(self):
        import contextlib
        import io
        self.compare([self.event()], [self.event()])
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(comparator.main([str(self.reference), str(self.candidate)]), 0)
            self.write(self.candidate, [self.event("different")])
            self.assertEqual(comparator.main([str(self.reference), str(self.candidate)]), 1)
            self.candidate.write_text("", encoding="utf-8")
            self.assertEqual(comparator.main([str(self.reference), str(self.candidate)]), 2)


if __name__ == "__main__":
    unittest.main()
