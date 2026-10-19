"""Exercise the explicit cross-repository native-plugin publication boundary."""
from pathlib import Path
import hashlib
import io
import json
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
            metadata['targetCommitish'] = 'b' * 40
            with self.assertRaisesRegex(ValueError, 'source commit differs'):
                nexus.fetch_release('v' + self.version, 'multiplayer', output)

    def test_default_framework_release_does_not_select_multiplayer(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools/nexus_release.py'), 'matrix', '--target', 'all'],
                                check=True, capture_output=True, text=True)
        self.assertEqual({row['product'] for row in json.loads(result.stdout)['include']}, {'framework', 'vortex'})

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


if __name__ == '__main__':
    unittest.main()
