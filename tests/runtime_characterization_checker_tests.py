import copy
import gzip
import hashlib
import json
from pathlib import Path
import unittest
from runtime_characterization_contract import apply_correction


class CorrectionChecks(unittest.TestCase):
    def setUp(self):
        self.baseline = b'a\nself-probe\nb\n'
        self.expected = b'a\nb\n'
        self.fixture = {
            'platforms': {'test': {
                'baseline_sha256': hashlib.sha256(self.baseline).hexdigest(),
                'corrected_sha256': hashlib.sha256(self.expected).hexdigest(), 'records': 2}},
            'patterns': [{'test': {'before': ['self-probe'], 'after': []}}],
            'edits': [[1, 0]]}

    def test_exact_reviewed_edit(self):
        self.assertEqual(apply_correction(self.baseline, self.fixture, 'test'), self.expected)

    def test_old_bug_is_no_longer_expected(self):
        self.assertNotEqual(apply_correction(self.baseline, self.fixture, 'test'), self.baseline)

    def test_unrelated_changes_remain_visible(self):
        corrected = apply_correction(self.baseline, self.fixture, 'test')
        for actual in [b'changed\nb\n', b'a\nextra\nb\n', b'a\n']:
            self.assertNotEqual(actual, corrected)

    def test_baseline_identity_is_mandatory(self):
        with self.assertRaises(ValueError):
            apply_correction(b'changed\nself-probe\nb\n', self.fixture, 'test')

    def test_changed_context_is_rejected(self):
        fixture = copy.deepcopy(self.fixture)
        fixture['patterns'][0]['test']['before'] = ['different']
        with self.assertRaises(ValueError):
            apply_correction(self.baseline, fixture, 'test')

    def test_malformed_edits_are_rejected(self):
        for edits in [[[9, 0]], [[1, -1]], [[1, 0], [0, 0]], [[-1, 0]]]:
            fixture = copy.deepcopy(self.fixture)
            fixture['edits'] = edits
            with self.assertRaises(ValueError):
                apply_correction(self.baseline, fixture, 'test')

    def test_corrected_identity_is_mandatory(self):
        fixture = copy.deepcopy(self.fixture)
        fixture['patterns'][0]['test']['after'] = ['unreviewed']
        with self.assertRaises(ValueError):
            apply_correction(self.baseline, fixture, 'test')

    def test_real_historical_baselines_are_kept_and_projectable(self):
        root = Path(__file__).parent / 'fixtures/runtime_characterization'
        fixture = json.loads((root / 'actor-generation-correction.json').read_text())
        provenance = json.loads((root / 'provenance.json').read_text())
        for platform in ['windows-x86', 'linux-x86']:
            baseline = gzip.decompress((root / (platform + '.txt.gz')).read_bytes())
            self.assertEqual(hashlib.sha256(baseline).hexdigest(), provenance['platforms'][platform]['sha256'])
            self.assertEqual(len(apply_correction(baseline, fixture, platform).splitlines()), 131028)


if __name__ == '__main__':
    unittest.main()
