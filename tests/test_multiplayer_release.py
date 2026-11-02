"""Exercise the explicit cross-repository native-plugin publication boundary."""
from pathlib import Path
import hashlib
import base64
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import nexus_release as nexus
import release_delivery as delivery


class MultiplayerReleaseTests(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'artifacts/multiplayer-release-tests'
        parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.version = '0.1.0-beta.14'
        self.commit = 'a' * 40
        self.archive = self.root / ('MMORPGTycoon2-Multiplayer-v' + self.version + '-windows-x64.zip')
        self.entries = {name: b'authored fixture' for name in ('MT2MultiplayerPlugin.dll', 'assets/multiplayer.png',
            'README.md', 'LICENSE.md', 'THIRD_PARTY_NOTICES.md', 'licenses/MinHook.txt',
            'licenses/DearImGui.txt', 'licenses/nlohmann-json.txt', 'licenses/SDL3.txt')}
        self.entries['mod.json'] = json.dumps({'id': 'doodesch.multiplayer', 'type': 'native',
            'entry': 'MT2MultiplayerPlugin.dll', 'api_version': '1.0.0', 'version': self.version}).encode()
        self.metadata = {'version': self.version, 'source_commit': self.commit, 'release_tag': 'v' + self.version,
            'package_mode': 'vortex', 'files': {name: {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                                             for name, data in self.entries.items()}}
        self.entries['package.json'] = json.dumps(self.metadata).encode()
        self.write()

    def write(self):
        with zipfile.ZipFile(self.archive, 'w') as stream:
            for name, data in self.entries.items():
                stream.writestr(name, data)

    def test_exact_plugin_inventory_and_content_hashes(self):
        self.assertEqual(nexus.archive_version(self.archive, 'multiplayer'), self.version)
        self.entries['MT2MultiplayerPlugin.dll'] = b'changed'
        self.write()
        with self.assertRaisesRegex(ValueError, 'checksum differs'):
            nexus.archive_version(self.archive, 'multiplayer')
        self.entries['MT2.exe'] = b'unexpected'
        self.write()
        with self.assertRaisesRegex(ValueError, 'inventory differs'):
            nexus.archive_version(self.archive, 'multiplayer')

    def test_explicit_target_uses_source_repo_and_exact_checksum(self):
        output = self.root / 'download'
        metadata = {'tagName': 'v' + self.version, 'isDraft': False, 'targetCommitish': self.commit,
            'body': 'Rejoin your friend and continue building. Both players need the same beta.',
            'url': 'https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases/tag/v' + self.version,
            'assets': [{'name': self.archive.name, 'size': self.archive.stat().st_size},
                       {'name': self.archive.name + '.sha256'}]}

        def source(repository, *args):
            self.assertEqual(repository, 'DooDesch-Mods/MMORPGTycoon2-Multiplayer')
            if args[:2] == ('release', 'view'):
                return json.dumps(metadata)
            self.assertEqual(args[:2], ('release', 'download'))
            self.assertIn(self.archive.name + '.sha256', args)
            shutil.copy2(self.archive, output / self.archive.name)
            (output / (self.archive.name + '.sha256')).write_text(
                hashlib.sha256(self.archive.read_bytes()).hexdigest() + '  ' + self.archive.name + '\n')

        with patch.object(nexus, 'gh_repository', side_effect=source), \
             patch.object(nexus, 'gh', return_value=json.dumps({'tagName': 'v0.2.0', 'isDraft': False})), \
             patch.object(nexus.urllib.request, 'urlopen', return_value=io.BytesIO(b'# Changelog\n\n## 0.1.0-beta.14\n\n- Native apps.\n')):
            plan = nexus.fetch_release('v' + self.version, 'multiplayer', output)
            self.assertEqual(plan['receipt_tag'], 'v0.2.0')
            self.assertEqual(plan['source_repository'], 'DooDesch-Mods/MMORPGTycoon2-Multiplayer')
            self.assertEqual(plan['release_notes'], metadata['body'])
            self.assertEqual(plan['notes_origin'], metadata['url'])
            self.assertEqual(plan['notes_sha256'], hashlib.sha256(metadata['body'].encode()).hexdigest())
            release_text = metadata['body']
            metadata['body'] = nexus.MULTIPLAYER_SUPPORT + '\n\n' + release_text
            transformed = nexus.fetch_release('v' + self.version, 'multiplayer', output)
            self.assertEqual(transformed['release_notes'], release_text)
            self.assertEqual(transformed['notes_source_sha256'], hashlib.sha256(metadata['body'].encode()).hexdigest())
            self.assertEqual(transformed['notes_sha256'], hashlib.sha256(release_text.encode()).hexdigest())
            with patch.object(nexus.urllib.request, 'urlopen', side_effect=AssertionError('Do not require an older tagged changelog')):
                nexus.fetch_release('v' + self.version, 'multiplayer', output)
            metadata['body'] = ''
            with self.assertRaisesRegex(ValueError, 'Published GitHub release notes'):
                nexus.fetch_release('v' + self.version, 'multiplayer', output)
            metadata['body'] = 'x' * 10001
            with self.assertRaisesRegex(ValueError, 'Published GitHub release notes'):
                nexus.fetch_release('v' + self.version, 'multiplayer', output)
            metadata['body'] = '- A published change.'
            metadata['targetCommitish'] = 'b' * 40
            with self.assertRaisesRegex(ValueError, 'source commit differs'):
                nexus.fetch_release('v' + self.version, 'multiplayer', output)

    def test_default_framework_release_does_not_select_multiplayer(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools/nexus_release.py'), 'matrix', '--target', 'all'],
                                check=True, capture_output=True, text=True)
        self.assertEqual({row['product'] for row in json.loads(result.stdout)['include']}, {'framework', 'vortex'})

    def test_canonical_schema2_package_without_legacy_provenance(self):
        self.metadata.pop('source_commit'); self.metadata.pop('release_tag')
        self.metadata.update(schema_version=2, candidate='framework-native-plugin', inputs={
            'mod_manifest_sha256': hashlib.sha256(self.entries['mod.json']).hexdigest(),
            'candidate_receipt_sha256': 'c' * 64})
        self.entries['package.json'] = json.dumps(self.metadata).encode(); self.write()
        self.assertEqual(nexus.archive_version(self.archive, 'multiplayer'), self.version)
        self.metadata['inputs']['mod_manifest_sha256'] = 'd' * 64
        self.entries['package.json'] = json.dumps(self.metadata).encode(); self.write()
        with self.assertRaisesRegex(ValueError, 'canonical package identity'):
            nexus.archive_version(self.archive, 'multiplayer')

    def test_schema2_does_not_ignore_conflicting_explicit_provenance(self):
        self.metadata.update(schema_version=2, candidate='framework-native-plugin', release_tag='v0.1.0-beta.99')
        self.entries['package.json'] = json.dumps(self.metadata).encode(); self.write()
        with self.assertRaisesRegex(ValueError, 'explicit source identity'):
            nexus.archive_version(self.archive, 'multiplayer')

    def test_canonical_package_requires_exact_github_tag_and_source_manifest(self):
        manifest = {'version': self.version}
        mod = json.loads(self.entries['mod.json'])
        repository = 'DooDesch-Mods/MMORPGTycoon2-Multiplayer'
        tag_commit = {'sha': self.commit}
        source = {'type': 'file', 'encoding': 'base64', 'content': base64.b64encode(self.entries['mod.json']).decode()}
        calls = []
        def api(*args):
            calls.append(args)
            return json.dumps(source if '/contents/' in args[1] else tag_commit)
        self.assertEqual(nexus.multiplayer_source_commit(manifest, mod, repository, 'v'+self.version, self.commit, api), self.commit)
        self.assertEqual(calls, [('api', f'repos/{repository}/commits/v{self.version}'),
                                 ('api', f'repos/{repository}/contents/mod.json?ref={self.commit}')])
        tag_commit['sha'] = 'b' * 40
        with self.assertRaisesRegex(ValueError, 'tag source commit differs'):
            nexus.multiplayer_source_commit(manifest, mod, repository, 'v'+self.version, self.commit, api)
        tag_commit['sha'] = self.commit
        source['content'] = base64.b64encode(json.dumps({**mod, 'version': '0.1.0-beta.99'}).encode()).decode()
        with self.assertRaisesRegex(ValueError, 'manifest differs'):
            nexus.multiplayer_source_commit(manifest, mod, repository, 'v'+self.version, self.commit, api)

    def test_receipt_is_read_from_cohort_but_announcement_points_to_mod(self):
        plan = {'product': 'multiplayer', 'version': self.version, 'tag': 'v' + self.version,
                'receipt_tag': 'v0.2.0', 'release_url': 'https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases/tag/v' + self.version,
                'nexus_url': 'https://www.nexusmods.com/mmorpgtycoon2/mods/28', 'notes': '- Native apps.'}
        with patch.object(delivery, 'gh', return_value=json.dumps({'assets': []})) as call:
            self.assertIsNone(delivery.remote_receipt(plan, 'receipt.json', self.root))
        self.assertEqual(call.call_args.args[:3], ('release', 'view', 'v0.2.0'))
        embed = delivery.discord_payload(plan)['embeds'][0]
        self.assertEqual(embed['url'], plan['release_url'])
        self.assertIn('MMORPGTycoon2-Multiplayer', embed['thumbnail']['url'])

    def test_poll_selects_latest_published_beta_without_other_targets(self):
        releases = [{'tag_name': 'v0.1.0-beta.' + number, 'draft': draft, 'prerelease': True}
                    for number, draft in [('27', False), ('28', False), ('29', True)]]
        releases.append({'tag_name': 'v0.2.0', 'draft': False, 'prerelease': False})
        with patch.object(nexus, 'gh_repository', return_value=json.dumps(releases)), \
             patch.object(nexus, 'gh', return_value=json.dumps({'assets': []})), patch.object(nexus, 'outputs') as output:
            nexus.poll_multiplayer()
        values = output.call_args.args[0]
        self.assertEqual(values['release_tag'], 'v0.1.0-beta.28')
        self.assertEqual(values['active'], 'true')
        self.assertEqual([row['product'] for row in json.loads(values['matrix'])['include']], ['multiplayer'])
        releases.append({'tag_name': 'v0.2.0-beta.1', 'draft': False, 'prerelease': True})
        with patch.object(nexus, 'gh_repository', return_value=json.dumps(releases)), \
             patch.object(nexus, 'gh', return_value=json.dumps({'assets': []})), patch.object(nexus, 'outputs') as output:
            nexus.poll_multiplayer()
        self.assertEqual(output.call_args.args[0]['release_tag'], 'v0.2.0-beta.1')

    def test_poll_skips_only_confirmed_matching_delivery_and_blocks_uncertain_send(self):
        tag = 'v0.1.0-beta.27'
        release = {'tag_name': tag, 'draft': False, 'prerelease': True, 'target_commitish': self.commit, 'assets': [{
            'name': 'MMORPGTycoon2-Multiplayer-' + tag + '-windows-x64.zip', 'digest': 'sha256:' + 'a' * 64}]}
        name = 'release-delivery-multiplayer-0.1.0-beta.27.json'
        receipt = {'status': 'confirmed', 'product': 'multiplayer', 'tag': tag, 'version': tag[1:], 'sha256': 'a' * 64,
                   'source_commit': self.commit}
        def central(*args):
            if args[:2] == ('release', 'view'):
                return json.dumps({'assets': [{'name': name}]})
            (Path(args[args.index('--dir') + 1]) / name).write_text(json.dumps(receipt))
            return ''
        with patch.object(nexus, 'gh_repository', return_value=json.dumps([release])), \
             patch.object(nexus, 'gh', side_effect=central), patch.object(nexus, 'outputs') as output, \
             patch.object(nexus.tempfile, 'tempdir', str(self.root)):
            nexus.poll_multiplayer()
            self.assertEqual(output.call_args.args[0], {'active': 'false'})
            receipt['status'] = 'pending'
            with self.assertRaisesRegex(ValueError, 'unconfirmed or differs'):
                nexus.poll_multiplayer()
            receipt['status'] = 'confirmed'
            receipt['sha256'] = 'b' * 64
            with self.assertRaisesRegex(ValueError, 'unconfirmed or differs'):
                nexus.poll_multiplayer()

    def test_poll_does_not_backfill_old_or_silent_releases(self):
        releases = [{'tag_name': 'v0.1.0-beta.' + number, 'draft': False, 'prerelease': True}
                    for number in ['25', '26']]
        with patch.object(nexus, 'gh_repository', return_value=json.dumps(releases)), \
             patch.object(nexus, 'gh') as central, patch.object(nexus, 'outputs') as output:
            nexus.poll_multiplayer()
        central.assert_not_called()
        self.assertEqual(output.call_args.args[0], {'active': 'false'})

    def test_delivered_changelog_recovery_reads_existing_text_without_writes(self):
        plan = {'product': 'multiplayer', 'status': 'already-present', 'version': '0.1.0-beta.27',
                'notes': 'Corrected GitHub copy', 'tag': 'v0.1.0-beta.27', 'sha256': 'a' * 64,
                'notes_origin': 'https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases/tag/v0.1.0-beta.27',
                'release_url': 'https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases/tag/v0.1.0-beta.27',
                'nexus_url': 'https://www.nexusmods.com/mmorpgtycoon2/mods/28'}
        calls = []
        def request(url, method='GET', **kwargs):
            calls.append(method)
            return {plan['version']: ['- Original delivered change.', '', 'Both players need the matching Runtime.']}
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}):
            delivery.sync_delivered_changelog(plan, self.root, request)
        self.assertEqual(calls, ['GET', 'GET'])
        drift = json.loads((self.root / 'changelog-drift.json').read_text())
        self.assertIn('Original delivered change.', drift['nexus_notes'])
        self.assertFalse(drift['notes_match_current_github'])
        self.assertFalse(drift['nexus_write'])
        self.assertEqual(drift['github_notes_sha256'], hashlib.sha256(plan['notes'].encode()).hexdigest())
        self.assertEqual(plan['notes'], 'Corrected GitHub copy')
        sent = []
        def discord(url, method='GET', body=None, **kwargs):
            self.assertEqual(method, 'POST')
            sent.append(body)
            return {'id': '123', 'channel_id': '456', 'embeds': body['embeds']}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), \
             patch.object(delivery, 'remote_receipt', return_value=None), patch.object(delivery, 'gh'):
            delivery.announce(plan, self.root, discord)
        self.assertEqual(sent[0]['embeds'][0]['description'], 'Corrected GitHub copy')
        with self.assertRaisesRegex(ValueError, 'existing Multiplayer'):
            delivery.delivered_changelog({**plan, 'status': 'ready'}, request)

    def test_future_file_description_is_not_pinned_to_an_old_runtime(self):
        description = nexus.targets()['multiplayer']['file_description']
        self.assertNotRegex(description, r'\d+\.\d+\.\d+')
        self.assertLessEqual(len(description), 255)


if __name__ == '__main__':
    unittest.main()
