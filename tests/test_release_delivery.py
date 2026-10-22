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
        self.assertEqual(notes.bbcode(text), '[list]\n[*][b]Fix[/b] menu "first"\n[*][url=https://example.org/help]Guide[/url]\n[/list]')
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

    def test_nexus_html_escapes_are_decoded_once_before_changelog_comparison(self):
        expected = "Settings > Mods & profiles: Vortex's menu"
        self.assertEqual(delivery.changelog_status({'0.1.2': ['Settings &gt; Mods &amp; profiles: Vortex&#39;s menu']},
                                                  '0.1.2', expected), 'confirmed')
        self.assertEqual(delivery.changelog_status({'0.1.2': ['Show &amp;gt; literally']},
                                                  '0.1.2', 'Show &gt; literally'), 'confirmed')
        with self.assertRaisesRegex(ValueError, 'different text'):
            delivery.changelog_status({'0.1.2': ['Settings &gt; Downloads']}, '0.1.2', 'Settings > Mods')

    def test_note_correction_cannot_publish_a_new_version_or_message(self):
        for status, update, message_id in [('ready', True, '123'), ('already-present', False, '123'),
                                          ('already-present', True, None), ('already-present', True, 'invalid')]:
            with self.subTest(status=status, update=update, message_id=message_id):
                with self.assertRaisesRegex(ValueError, 'existing'):
                    delivery.corrected_plan({**self.plan, 'status': status}, '- Correct text.', update, message_id)

    def test_note_correction_preserves_release_identity_and_checks_existing_changelog(self):
        plan = {**self.plan, 'status': 'already-present'}
        result = delivery.corrected_plan(plan, '### Fixed\n\n- **Build** together.', True, '123')
        for key in ('product', 'version', 'tag', 'sha256', 'release_url'):
            self.assertEqual(result[key], plan[key])
        self.assertEqual(plan['notes'], self.plan['notes'])
        self.assertEqual(delivery.changelog_status({plan['version']: ['Fixed', '- Build together.']},
                                                  plan['version'], result['notes_plain']), 'confirmed')
        self.assertIn('**Build** together.', delivery.discord_payload(result)['embeds'][0]['description'])
        self.assertIs(delivery.corrected_plan(plan, ''), plan)

    def test_missing_changelog_readback_is_a_failure(self):
        with patch.dict(os.environ, {'NEXUSMODS_API_KEY': 'fixture'}):
            with self.assertRaisesRegex(RuntimeError, 'readback is missing'):
                delivery.sync_changelog(self.plan, self.directory, lambda *args, **kwargs: {})
        self.assertFalse((self.directory / 'changelog.json').exists())

    def test_discord_payload_uses_correct_destination_and_no_mentions(self):
        payload = delivery.discord_payload(self.plan)
        self.assertEqual(payload['allowed_mentions'], {'parse': []})
        self.assertIn('/mmorpgtycoon2/mods/25', payload['embeds'][0]['fields'][0]['value'])
        self.assertIn('thumbnail', payload['embeds'][0])
        self.assertNotIn('image', payload['embeds'][0])
        self.plan['notes'] = '### Added\n\n- Edit monsters with a friend.\n\n### Fixed\n\n- Switch tools after editing.'
        self.assertEqual(delivery.discord_payload(self.plan)['embeds'][0]['description'],
            '**Added**\n\n- Edit monsters with a friend.\n\n**Fixed**\n\n- Switch tools after editing.')
        self.plan['notes'] = 'x ' * 3000
        self.assertLessEqual(len(delivery.discord_payload(self.plan)['embeds'][0]['description']), 1400)
        for invalid in ('http://discord.com/api/webhooks/1/token', 'https://evil.example/api/webhooks/1/token'):
            with self.assertRaises(ValueError): delivery.webhook_url(invalid)

    def test_discord_uses_the_same_changelog_without_a_separate_copy_file(self):
        for version in ('0.1.2', '0.9.0'):
            self.plan.update(version=version, notes='### Changed\n\n- Build together.')
            self.assertEqual(delivery.discord_payload(self.plan)['embeds'][0]['description'],
                '**Changed**\n\n- Build together.')

    def test_multiplayer_title_uses_readable_beta_number(self):
        self.plan.update(product='multiplayer', version='0.1.0-beta.15')
        self.assertEqual(delivery.announcement_title(self.plan), 'MMORPG Tycoon 2 Multiplayer - Early Beta 15')

    def test_pending_discord_send_blocks_replay(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')}, 'status': 'pending'}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous):
            with self.assertRaisesRegex(RuntimeError, 'unconfirmed send intent'):
                delivery.announce(self.plan, self.directory, lambda url, **kwargs: {'guild_id': '1', 'channel_id': '2'})

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
        self.assertEqual(events, ['pending', 'POST', 'acknowledged', 'confirmed'])

    def test_confirmed_discord_message_is_read_instead_of_posted(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')},
                    'status': 'confirmed', 'message_id': '123', 'embeds': delivery.canonical_embeds(delivery.discord_payload(self.plan)['embeds'])}
        def request(url, **kwargs):
            self.assertTrue(url.endswith('/messages/123'))
            return {'id': '123', 'embeds': delivery.discord_payload(self.plan)['embeds']}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous):
            delivery.announce(self.plan, self.directory, request)

    def test_discord_added_transport_fields_do_not_change_content_identity(self):
        expected = delivery.discord_payload(self.plan)['embeds']
        returned = json.loads(json.dumps(expected))
        returned[0]['type'] = 'rich'
        returned[0]['fields'][0]['inline'] = False
        returned[0]['thumbnail'].update(proxy_url='https://images-ext-1.discordapp.net/proxy/fixture', width=128, height=128)
        self.assertEqual(delivery.canonical_embeds(expected), delivery.canonical_embeds(returned))

    def test_existing_message_reconciles_pending_intent_without_post(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')}, 'status': 'pending'}
        def request(url, **kwargs):
            self.assertTrue(url.endswith('/messages/123'))
            return {'id': '123', 'channel_id': '456', 'embeds': delivery.discord_payload(self.plan)['embeds']}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous), patch.object(delivery, 'gh') as gh:
            delivery.announce(self.plan, self.directory, request, message_id='123')
        self.assertEqual(previous['status'], 'confirmed')
        self.assertEqual(gh.call_count, 1)

    def test_mismatched_discord_ack_keeps_message_id_for_recovery(self):
        message = {'id': '123', 'channel_id': '456', 'embeds': []}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=None), patch.object(delivery, 'gh'):
            with self.assertRaisesRegex(ValueError, 'different release content'):
                delivery.announce(self.plan, self.directory, lambda *args, **kwargs: message)
        stored = json.loads((self.directory / 'release-delivery-framework-0.1.2.json').read_text())
        self.assertEqual((stored['status'], stored['message_id']), ('acknowledged', '123'))

    def test_explicit_update_edits_existing_release_then_reads_back(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')}, 'status': 'pending'}
        message = {'id': '123', 'channel_id': '456', 'embeds': delivery.discord_payload(self.plan)['embeds']}
        message['embeds'][0]['description'] = 'Old release copy'
        methods = []
        def request(url, method='GET', body=None, **kwargs):
            self.assertTrue(url.endswith('/messages/123'))
            methods.append(method)
            if method == 'PATCH': message['embeds'] = body['embeds']
            return message
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous), patch.object(delivery, 'gh'):
            delivery.announce(self.plan, self.directory, request, message_id='123', update=True)
        self.assertEqual(methods, ['GET', 'PATCH', 'GET'])
        self.assertEqual(previous['status'], 'confirmed')

    def test_update_rejects_a_message_for_a_different_release(self):
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')}, 'status': 'pending'}
        message = {'id': '123', 'embeds': [{'title': 'Another product', 'url': self.plan['release_url']}]}
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous):
            with self.assertRaisesRegex(ValueError, 'exact product release'):
                delivery.announce(self.plan, self.directory, lambda url, **kwargs: message, message_id='123', update=True)

    def test_update_accepts_exact_legacy_title_but_rejects_other_release_url(self):
        self.plan.update(product='multiplayer', version='0.1.0-beta.15', tag='v0.1.0-beta.15',
            release_url='https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases/tag/v0.1.0-beta.15')
        previous = {**{key: self.plan[key] for key in ('product', 'version', 'tag', 'sha256')}, 'status': 'confirmed', 'message_id': '123'}
        message = {'id': '123', 'channel_id': '456', 'embeds': [{
            'title': 'MMORPG Tycoon 2 Multiplayer - Early Beta 0.1.0-beta.15', 'url': self.plan['release_url']}]}
        methods = []
        def request(url, method='GET', body=None, **kwargs):
            methods.append(method)
            if method == 'PATCH': message['embeds'] = body['embeds']
            return message
        with patch.dict(os.environ, {'DISCORD_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/1/fixture'}), patch.object(delivery, 'remote_receipt', return_value=previous), patch.object(delivery, 'gh'):
            delivery.announce(self.plan, self.directory, request, message_id='123', update=True)
            self.assertEqual(methods, ['GET', 'PATCH', 'GET'])
            message['embeds'][0]['url'] = self.plan['release_url'].replace('beta.15', 'beta.14')
            with self.assertRaisesRegex(ValueError, 'exact product release'):
                delivery.announce(self.plan, self.directory, request, message_id='123', update=True)
        self.assertEqual(methods, ['GET', 'PATCH', 'GET', 'GET'])


if __name__ == '__main__': unittest.main()
