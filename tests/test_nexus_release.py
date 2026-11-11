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

    def test_tag_names_the_product_and_must_match_the_archive_version(self):
        self.assertEqual(nexus.product_for('v0.5.9'), ('framework', '0.5.9'))
        self.assertEqual(nexus.product_for('vortex-v0.2.1'), ('vortex', '0.2.1'))
        with self.assertRaises(ValueError):nexus.product_for('multiplayer-v1.0.0')
        test_root = nexus.ROOT / 'artifacts/nexus-tests'
        test_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=test_root) as folder:
            directory = Path(folder)
            path = directory / 'MMORPGTycoon2-Vortex-0.2.1.zip'
            with zipfile.ZipFile(path, 'w') as archive:
                archive.writestr('info.json', json.dumps({'gameId': 4452, 'id': 'game-mmorpgtycoon2', 'version': '0.2.1'}))
            (directory / 'SHA256SUMS.txt').write_text(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n')
            self.assertEqual(nexus.local_release('vortex-v0.2.1', directory)['version'], '0.2.1')
            with self.assertRaisesRegex(ValueError, 'differs from the archive version'):
                nexus.local_release('vortex-v0.2.2', directory)


class ChangelogTests(unittest.TestCase):
    plan = {'product': 'framework', 'version': '0.1.2', 'notes_plain': '- Fix menu.\n- English default.'}

    def test_product_sections_are_exact_and_missing_or_duplicates_fail(self):
        text = '## 0.1.2\n\n- Framework\n## Vortex extension 0.1.2\n\n- Extension\n'
        self.assertEqual(nexus.section(text, 'framework', '0.1.2'), '- Framework')
        self.assertEqual(nexus.section(text, 'vortex', '0.1.2'), '- Extension')
        for invalid in ('## 0.1.20\n- Wrong', text + '\n## 0.1.2\n- Duplicate', '## 0.1.2\n'):
            with self.assertRaises(ValueError):nexus.section(invalid, 'framework', '0.1.2')

    def test_format_preserves_quotes_newlines_and_link_targets(self):
        text = '- **Fix** `menu` "first"\n- [Guide](https://example.org/help)'
        self.assertEqual(nexus.plain(text), '- Fix menu "first"\n- Guide (https://example.org/help)')
        self.assertEqual(nexus.bbcode(text), '[list]\n[*][b]Fix[/b] menu "first"\n[*][url=https://example.org/help]Guide[/url]\n[/list]')

    def test_changelog_write_is_read_back_and_never_repeated(self):
        calls = []
        published = {}
        def request(url, method='GET', body=None, headers=None):
            calls.append(method)
            if method == 'POST':published[body['version']] = [body['changelog']]
            return published
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}), patch('builtins.print'):
            nexus.sync_changelog(self.plan, request)
            nexus.sync_changelog(self.plan, request)
        self.assertEqual(calls, ['GET', 'POST', 'GET', 'GET'])

    def test_conflicting_changelog_stops_before_additive_write(self):
        def request(url, **kwargs):
            self.assertNotIn('body', kwargs)
            return {'0.1.2': ['Different release notes']}
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}):
            with self.assertRaisesRegex(ValueError, 'different text'):nexus.sync_changelog(self.plan, request)

    def test_nexus_html_escapes_are_decoded_once_before_changelog_comparison(self):
        expected = "Settings > Mods & profiles: Vortex's menu"
        self.assertEqual(nexus.changelog_status({'0.1.2': ['Settings &gt; Mods &amp; profiles: Vortex&#39;s menu']}, '0.1.2', expected), 'confirmed')
        self.assertEqual(nexus.changelog_status({'0.1.2': ['Show &amp;gt; literally']}, '0.1.2', 'Show &gt; literally'), 'confirmed')
        with self.assertRaisesRegex(ValueError, 'different text'):
            nexus.changelog_status({'0.1.2': ['Settings &gt; Downloads']}, '0.1.2', 'Settings > Mods')

    def test_missing_changelog_readback_is_a_failure(self):
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}), patch('builtins.print'):
            with self.assertRaisesRegex(RuntimeError, 'readback is missing'):
                nexus.sync_changelog(self.plan, lambda *args, **kwargs: {})


if __name__ == '__main__':unittest.main()
