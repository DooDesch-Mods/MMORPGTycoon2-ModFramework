"""Keep route metadata and the actual compiled catalog in agreement."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MultiplayerCatalogTests(unittest.TestCase):
    def setUp(self):
        self.targets = json.loads((ROOT / 'adapters/mt2/multiplayer-catalog.json').read_text(encoding='utf-8'))['targets']

    def test_compiled_catalog_matches_metadata_without_aliases(self):
        source = (ROOT / 'adapters/mt2/multiplayer_catalog.inc').read_text(encoding='utf-8')
        compiled = [(name, int(rva, 16), shared == 'true') for name, rva, shared in
                    re.findall(r'catalog\.catalog\("([^"]+)",(0x[0-9a-f]+),(true|false)\);', source)]
        expected = [(row['name'], row['rva'], row['shared']) for row in self.targets]
        self.assertEqual(compiled, expected)
        self.assertEqual(len({name for name, _, _ in compiled}), len(compiled))
        self.assertEqual(len({rva for _, rva, _ in compiled}), len(compiled))

    def test_monster_release_has_one_private_named_route(self):
        matches = [row for row in self.targets if row['rva'] == 0x5bf3c0]
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]['name'], 'mp.monster-release-capture.5bf3c0')
        self.assertIs(matches[0]['shared'], False)
        self.assertEqual(matches[0]['groups'][0]['feature'], 'Monster Release capture')
        self.assertEqual(matches[0]['groups'][0]['phase'], 'typed_action_runtime::start, bound Mainthread, once per process')


    def test_ordered_original_observers_have_exclusive_named_routes(self):
        for rva, name in [(0x53a240, 'mp.ordered-tutorial-undo-clear.53a240'),
                          (0x559ee0, 'mp.ordered-clearance-make-scenery.559ee0')]:
            matches = [row for row in self.targets if row['rva'] == rva]
            self.assertEqual(len(matches), 1)
            self.assertEqual(matches[0]['name'], name)
            self.assertIs(matches[0]['shared'], False)


    def test_current_catalog_preserves_public_021_routes(self):
        prior = json.loads((ROOT / 'tests/fixtures/multiplayer-catalog-v0.2.1.json').read_text(encoding='utf-8'))['targets']
        previous = {row['name'] for row in prior}
        self.assertEqual([row for row in self.targets if row['name'] in previous], prior)
        self.assertEqual(len(self.targets), 290)
        self.assertEqual(sum(row['shared'] for row in self.targets), 11)

    def test_toon_camera_gate_is_one_exact_exclusive_internal_site(self):
        matches = [row for row in self.targets if row['rva'] == 0x3ea462]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.toon-camera-schedule-gate.3ea462')
        self.assertIs(row['shared'], False)
        guard = row['code_guard']
        self.assertEqual(guard['bytes'], '80bbc0010000000f847303000080bb08080000000f84660300000f1f4000')
        self.assertEqual(guard['length'], 30)
        self.assertEqual(guard['sha256'], '3d45300ee4bf4b417a337091479e37bfd119ac92bc240759c02710e2b7d0d99f')
        self.assertEqual(guard['deferred_rva'], 0x3ea7e2)
        self.assertEqual(guard['full_rva'], 0x3ea480)

    def test_travel_rotation_gate_preserves_the_exact_original_branch(self):
        matches = [row for row in self.targets if row['rva'] == 0x3b7b37]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.toon-travel-rotation-gate.3b7b37')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['code_guard']['bytes'], '0f8403060000')
        self.assertEqual(row['code_guard']['fallthrough_rva'], 0x3b7b3d)
        self.assertEqual(row['code_guard']['branch_rva'], 0x3b8140)
        self.assertEqual(row['body_guard']['rva'], 0x3b78b0)
        self.assertEqual(row['body_guard']['length'], 5120)
        self.assertEqual(row['body_guard']['sha256'], '5227570ec1fc062edf107ffbb456d83234e4fba1e5a70e270e2e107fc2d58500')

    def test_attribute_cadence_gate_preserves_the_exact_original_branch(self):
        matches = [row for row in self.targets if row['rva'] == 0x3a0942]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.toon-attribute-cadence-gate.3a0942')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['code_guard']['bytes'], '0f85380b0000')
        self.assertEqual(row['code_guard']['fallthrough_rva'], 0x3a0948)
        self.assertEqual(row['code_guard']['branch_rva'], 0x3a1480)
        self.assertEqual(row['window_guard']['length'], 30)
        self.assertEqual(row['window_guard']['sha256'], 'daf271c95120774c9f0da8cf3696e87a1ce61f7053aa60371753fe406104e14b')
        self.assertEqual(row['body_guard']['rva'], 0x3a0350)
        self.assertEqual(row['body_guard']['length'], 6816)
        self.assertEqual(row['body_guard']['sha256'], 'e994c1ef7cf8ff10fe463e8def99af65798238dce88481dbd3dc82838c828b79')


if __name__ == '__main__':
    unittest.main()
