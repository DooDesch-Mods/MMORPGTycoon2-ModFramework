import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import urllib.error
import zipfile
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('nexus_release', Path(__file__).resolve().parents[1] / 'tools/nexus_release.py')
nexus = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nexus)


class NexusReleaseTests(unittest.TestCase):
    def setUp(self):
        self.target = nexus.targets()['framework']
        self.versions = [{'id': '1', 'file': {'id': self.target['file_id']}, 'version': '0.1.0', 'category': 'main'}]
        self.calls = []

    def get(self, endpoint):
        self.calls.append(endpoint)
        if endpoint.startswith('/games/'):
            return {'id': self.target['mod_uid'], 'game_id': str(self.target['game_id']), 'game_scoped_id': str(self.target['page_id'])}
        if endpoint.startswith('/mods/'):
            return {'mod_files': [{'id': self.target['file_id']}]}
        return {'versions': self.versions}

    def test_global_ids_and_file_destinations_are_distinct(self):
        values = nexus.targets()
        self.assertEqual(values['framework']['mod_uid'], '19121194401817')
        self.assertEqual(values['vortex']['mod_uid'], '9856949946785')
        self.assertNotEqual(values['framework']['file_id'], values['vortex']['file_id'])
        self.assertEqual(nexus.check_destination(self.target, '0.1.1', self.get), 'ready')
        self.assertEqual(self.calls, ['/games/mmorpgtycoon2/mods/25', '/mods/19121194401817/files', '/mod-files/8170305/versions'])

    def test_existing_version_is_skipped_before_any_upload(self):
        self.assertEqual(nexus.check_destination(self.target, '0.1.0', self.get), 'already-present')

    def test_older_version_is_rejected(self):
        self.versions[0]['version'] = '0.2.0'
        with self.assertRaisesRegex(ValueError, 'older'):
            nexus.check_destination(self.target, '0.1.1', self.get)

    def test_wrong_page_or_file_does_not_pass_preflight(self):
        def wrong_page(endpoint):
            row = self.get(endpoint)
            if endpoint.startswith('/games/'):row['id'] = '25'
            return row
        with self.assertRaisesRegex(ValueError, 'different mod/game'):
            nexus.check_destination(self.target, '0.1.1', wrong_page)
        def wrong_file(endpoint):
            return {'mod_files': [{'id': '8170257'}]} if endpoint.startswith('/mods/') else self.get(endpoint)
        with self.assertRaisesRegex(ValueError, 'does not belong'):
            nexus.check_destination(self.target, '0.1.1', wrong_file)

    def test_foreign_version_chain_is_rejected(self):
        self.versions[0]['file']['id'] = '8170257'
        with self.assertRaisesRegex(ValueError, 'another file'):
            nexus.check_destination(self.target, '0.1.1', self.get)

    def test_semver_orders_prereleases_without_float_conversion(self):
        sequence = ['0.1.0-alpha.2', '0.1.0-alpha.10', '0.1.0-beta', '0.1.0', '0.1.10', '0.2.0']
        self.assertEqual(sorted(reversed(sequence), key=nexus.version_key), sequence)

    def test_checksums_require_an_unambiguous_exact_filename(self):
        digest = 'a' * 64
        self.assertEqual(nexus.checksum_for(digest + '  file.zip\n', 'file.zip'), digest)
        with self.assertRaises(ValueError):nexus.checksum_for(digest + '  other.zip', 'file.zip')
        with self.assertRaises(ValueError):nexus.checksum_for((digest + '  file.zip\n') * 2, 'file.zip')

    def test_framework_payload_tampering_and_unsafe_paths_are_rejected(self):
        test_root = nexus.ROOT / 'artifacts/nexus-tests'
        test_root.mkdir(parents=True, exist_ok=True)
        if not test_root.resolve().is_relative_to(nexus.ROOT.resolve()):raise RuntimeError('Test directory escaped workspace')
        with tempfile.TemporaryDirectory(dir=test_root) as folder:
            path = Path(folder) / 'framework.zip'
            name = 'mt2fw/test.txt'
            metadata = {'version': '0.1.0', 'files': {name: hashlib.sha256(b'original').hexdigest()}}
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('framework-package.json', json.dumps(metadata));archive.writestr(name, 'changed')
            with self.assertRaisesRegex(ValueError, 'checksum differs'):nexus.archive_version(path, 'framework')
            with zipfile.ZipFile(path, 'w') as archive:archive.writestr('../escape', 'fixture')
            with self.assertRaisesRegex(ValueError, 'Unsafe ZIP path'):nexus.archive_version(path, 'framework')

    def test_api_error_does_not_expose_credentials_or_response_body(self):
        error = urllib.error.HTTPError('https://api.nexusmods.com/v3/mods/1/files', 403, 'forbidden', {}, None)
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'private-fixture-key'}), patch.object(nexus.urllib.request, 'urlopen', side_effect=error):
            with self.assertRaisesRegex(RuntimeError, 'HTTP 403') as caught:nexus.api_get('/mods/1/files')
        self.assertNotIn('private-fixture-key', str(caught.exception))

    def test_short_file_description_does_not_truncate_changelog(self):
        target = nexus.targets()['multiplayer']
        self.assertLessEqual(len(target['file_description']), 255)
        notes = '# Changelog\n\n## 0.1.0-beta.23\n\n### Fixed\n\n- ' + 'Detailed release note. ' * 30
        plan = {'product': 'multiplayer', 'tag': 'v0.1.0-beta.23', 'version': '0.1.0-beta.23',
                'filename': 'release.zip', 'sha256': 'a' * 64, 'notes_source': notes}
        root = nexus.ROOT / 'artifacts/nexus-tests'
        root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=root) as directory:
            args = SimpleNamespace(product='multiplayer', tag=plan['tag'], directory=directory, optional=False)
            environment = {'NEXUS_FILE_ID': target['file_id'], 'NEXUS_MOD_ID': target['mod_uid']}
            with patch.dict(os.environ, environment), patch.object(nexus, 'fetch_release', return_value=plan), \
                 patch.object(nexus, 'check_destination', return_value='ready'), patch.object(nexus, 'outputs') as outputs, \
                 patch('builtins.print'):
                nexus.prepare(args)
            saved = json.loads((Path(directory) / 'plan.json').read_text(encoding='utf-8'))
            self.assertEqual(outputs.call_args.args[0]['description'], target['file_description'])
            self.assertGreater(len(saved['notes_plain']), 255)
            self.assertIn('Detailed release note.', saved['notes_bbcode'])

    def test_invalid_configured_file_description_blocks_before_upload(self):
        target = {**nexus.targets()['multiplayer'], 'file_description': 'x' * 256}
        plan = {'product': 'multiplayer', 'tag': 'v0.1.0-beta.23', 'version': '0.1.0-beta.23',
                'notes_source': '# Changelog\n\n## 0.1.0-beta.23\n\n- A change.'}
        environment = {'NEXUS_FILE_ID': target['file_id'], 'NEXUS_MOD_ID': target['mod_uid']}
        with patch.dict(os.environ, environment), patch.object(nexus, 'targets', return_value={'multiplayer': target}), \
             patch.object(nexus, 'fetch_release', return_value=plan), patch.object(nexus, 'check_destination', return_value='ready'):
            with self.assertRaisesRegex(ValueError, 'at most 255'):
                nexus.prepare(SimpleNamespace(product='multiplayer', tag=plan['tag'], directory='unused', optional=False))


if __name__ == '__main__':unittest.main()
