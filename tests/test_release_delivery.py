import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import release_delivery as delivery
import release_notes as notes
import nexus_release as nexus


class ReleaseDeliveryTests(unittest.TestCase):
    def setUp(self):
        self.plan = {'product': 'framework', 'version': '0.1.2', 'tag': 'v0.1.2',
            'sha256': 'a' * 64, 'notes': '- Fix menu.\n- English default.',
            'notes_plain': '- Fix menu.\n- English default.',
            'release_url': 'https://github.com/' + nexus.REPOSITORY + '/releases/tag/v0.1.2',
            'nexus_url': 'https://www.nexusmods.com/mmorpgtycoon2/mods/25'}
        test_root = nexus.ROOT / 'artifacts/release-tests'
        test_root.mkdir(parents=True, exist_ok=True)
        if not test_root.resolve().is_relative_to(nexus.ROOT.resolve()):
            raise RuntimeError('Test directory escaped workspace')
        self.folder = tempfile.TemporaryDirectory(dir=test_root)
        self.addCleanup(self.folder.cleanup)
        self.directory = Path(self.folder.name)

    def test_product_sections_are_exact_and_missing_or_duplicates_fail(self):
        text = '## 0.1.2\n\n- Framework\n## Vortex extension 0.1.2\n\n- Extension\n'
        self.assertEqual(notes.section(text, 'framework', '0.1.2'), '- Framework')
        self.assertEqual(notes.section(text, 'vortex', '0.1.2'), '- Extension')
        for invalid in ('## 0.1.20\n- Wrong', text + '\n## 0.1.2\n- Duplicate', '## 0.1.2\n'):
            with self.assertRaises(ValueError): notes.section(invalid, 'framework', '0.1.2')

    def test_format_preserves_quotes_newlines_and_link_targets(self):
        text = '- **Fix** `menu` "first"\n- [Guide](https://example.org/help)'
        self.assertEqual(notes.plain(text), '- Fix menu "first"\n- Guide (https://example.org/help)')
        self.assertEqual(notes.bbcode(text), '- [b]Fix[/b] menu "first"\n- [url=https://example.org/help]Guide[/url]')
        destination = self.directory / 'outputs'
        with patch.dict(os.environ, {'GITHUB_OUTPUT': str(destination)}): nexus.outputs({'description': text})
        stored = destination.read_text()
        delimiter = stored.splitlines()[0].split('<<')[1]
        self.assertEqual(stored, f'description<<{delimiter}\n{text}\n{delimiter}\n')

    def test_changelog_write_is_read_back_and_never_repeated(self):
        calls = []
        published = {}
        def request(url, method='GET', body=None, headers=None):
            calls.append(method)
            if method == 'POST': published[body['version']] = [body['changelog']]
            return published
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}):
            delivery.sync_changelog(self.plan, self.directory, request)
            delivery.sync_changelog(self.plan, self.directory, request)
        self.assertEqual(calls, ['GET', 'POST', 'GET', 'GET'])
        self.assertEqual(json.loads((self.directory / 'changelog.json').read_text())['status'], 'confirmed')

    def test_conflicting_changelog_stops_before_additive_write(self):
        def request(url, **kwargs):
            self.assertNotIn('body', kwargs)
            return {'0.1.2': ['Different release notes']}
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}):
            with self.assertRaisesRegex(ValueError, 'different text'):
                delivery.sync_changelog(self.plan, self.directory, request)

    def test_missing_changelog_readback_is_a_failure(self):
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}):
            with self.assertRaisesRegex(RuntimeError, 'readback is missing'):
                delivery.sync_changelog(self.plan, self.directory, lambda *args, **kwargs: {})
        self.assertFalse((self.directory / 'changelog.json').exists())

    def test_discord_payload_uses_correct_destination_and_no_mentions(self):
        payload = delivery.discord_payload(self.plan)
        self.assertEqual(payload['allowed_mentions'], {'parse': []})
        self.assertIn('/mmorpgtycoon2/mods/25', payload['embeds'][0]['fields'][0]['value'])
        self.plan['notes'] = 'x ' * 3000
        self.assertLessEqual(len(delivery.discord_payload(self.plan)['embeds'][0]['description']), 1400)
        for invalid in ('http://discord.com/api/webhooks/1/token', 'https://evil.example/api/webhooks/1/token'):
            with self.assertRaises(ValueError): delivery.webhook_url(invalid)

    def test_pending_discord_send_blocks_replay(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')}, 'status': 'pending'}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous):
            with self.assertRaisesRegex(RuntimeError, 'unconfirmed send intent'):
                delivery.announce(self.plan, self.directory, lambda *args, **kwargs: self.fail('Must not send again'))

    def test_discord_intent_precedes_post_and_ack_is_retained(self):
        events = []
        def gh(*args):
            receipt = json.loads(Path(args[3]).read_text())
            events.append(receipt['status'])
        def request(*args, **kwargs):
            self.assertEqual(events, ['pending'])
            events.append('POST')
            return {'id': '123', 'channel_id': '456', 'embeds': delivery.discord_payload(self.plan)['embeds']}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=None), patch.object(delivery, 'gh', side_effect=gh):
            delivery.announce(self.plan, self.directory, request)
        self.assertEqual(events, ['pending', 'POST', 'confirmed'])

    def test_confirmed_discord_message_is_read_instead_of_posted(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')},
                    'status': 'confirmed', 'message_id': '123', 'embeds': []}
        def request(url, **kwargs):
            self.assertTrue(url.endswith('/messages/123'))
            return {'id': '123', 'embeds': []}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous):
            delivery.announce(self.plan, self.directory, request)


if __name__ == '__main__': unittest.main()
