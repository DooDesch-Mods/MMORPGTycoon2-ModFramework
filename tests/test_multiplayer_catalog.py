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


    def test_current_catalog_preserves_public_021_routes(self):
        prior = json.loads((ROOT / 'tests/fixtures/multiplayer-catalog-v0.2.1.json').read_text(encoding='utf-8'))['targets']
        previous = {row['name'] for row in prior}
        self.assertEqual([row for row in self.targets if row['name'] in previous], prior)
        self.assertEqual(len(self.targets), 192)
        self.assertEqual(sum(row['shared'] for row in self.targets), 11)


if __name__ == '__main__':
    unittest.main()
