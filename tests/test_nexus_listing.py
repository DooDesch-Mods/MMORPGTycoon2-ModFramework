import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import nexus_listing as listing


class NexusListingTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'artifacts/nexus-listing-tests'
        parent.mkdir(parents=True, exist_ok=True)
        temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        source = ROOT / 'integrations/nexus/listings/multiplayer'
        self.plan = {**json.loads((source / 'release.json').read_text()), 'product': 'multiplayer',
                     'status': 'already-present', 'nexus_url': 'https://www.nexusmods.com/mmorpgtycoon2/mods/28'}
        self.summary = (source / 'summary.txt').read_text(encoding='utf-8').strip()
        self.body = (source / 'description.bbcode.txt').read_text(encoding='utf-8').strip()
        self.mod = {'id': '19121194401820', 'game_id': '4452', 'game_scoped_id': '28',
                    'version': '0.1.0-beta.25', 'name': 'Keep this title', 'author': 'Keep these credits',
                    'gallery_images': [{'url': 'existing-image'}], 'description': 'Previous description'}
        self.current_summary = 'Previous summary'
        self.calls = []
        self.ignore_patch = False
        self.change_title = False
        self.write_plan()
        environment = patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'test-key'})
        environment.start()
        self.addCleanup(environment.stop)

    def write_plan(self):
        (self.directory / 'plan.json').write_text(json.dumps(self.plan), encoding='utf-8')

    def request(self, url, method='GET', body=None, headers=None):
        self.calls.append((url, method, body))
        if method == 'PATCH':
            self.assertEqual(set(body), {'summary', 'description'})
            if not self.ignore_patch:
                self.mod['description'] = body['description']
                self.current_summary = body['summary']
            if self.change_title:
                self.mod['name'] = 'Unexpected title'
            return None
        if '/v1/' in url:
            return {'mod_id': 28, 'version': self.mod['version'], 'summary': self.current_summary}
        return {'data': copy.deepcopy(self.mod)}

    def test_updates_only_prepared_text_and_confirms_readback(self):
        receipt = listing.update_listing(self.directory, self.request)
        self.assertEqual(receipt['status'], 'confirmed')
        self.assertTrue(receipt['changed'])
        self.assertEqual(self.mod['description'], self.body)
        self.assertEqual(self.current_summary, self.summary)
        self.assertEqual(sum(method == 'PATCH' for _, method, _ in self.calls), 1)
        self.assertEqual(self.mod['name'], 'Keep this title')
        self.assertEqual(self.mod['gallery_images'], [{'url': 'existing-image'}])

    def test_matching_listing_does_not_repeat_a_write(self):
        self.mod['description'] = self.body
        self.current_summary = self.summary
        receipt = listing.update_listing(self.directory, self.request)
        self.assertFalse(receipt['changed'])
        self.assertFalse(any(method == 'PATCH' for _, method, _ in self.calls))

    def test_wrong_release_or_mod_blocks_before_write(self):
        self.plan['sha256'] = 'b' * 64
        self.write_plan()
        with self.assertRaisesRegex(ValueError, 'Prepared listing release differs'):
            listing.update_listing(self.directory, self.request)
        self.assertEqual(self.calls, [])

    def test_wrong_destination_blocks_before_write(self):
        self.mod['id'] = '19121194401817'
        with self.assertRaisesRegex(ValueError, 'identity/version differs'):
            listing.update_listing(self.directory, self.request)
        self.assertFalse(any(method == 'PATCH' for _, method, _ in self.calls))

    def test_success_response_without_saved_text_is_not_success(self):
        self.ignore_patch = True
        with self.assertRaisesRegex(RuntimeError, 'acknowledgement differs'):
            listing.update_listing(self.directory, self.request)
        self.assertFalse((self.directory / 'listing.json').exists())

    def test_unrelated_field_change_is_reported(self):
        self.change_title = True
        with self.assertRaisesRegex(RuntimeError, 'unrelated field: name'):
            listing.update_listing(self.directory, self.request)


if __name__ == '__main__':
    unittest.main()
