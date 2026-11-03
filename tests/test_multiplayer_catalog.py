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
        self.assertEqual(len(self.targets), 297)
        self.assertEqual(sum(row['shared'] for row in self.targets), 12)

    def test_npc_camera_gate_preserves_one_exact_exclusive_original_branch(self):
        matches = [row for row in self.targets if row['rva'] == 0x3c8db7]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.npc-camera-gate.3c8db7')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['code_guard'], {
            'offset': 0, 'length': 9, 'bytes': '8038000f85b8000000',
            'sha256': '9be443f68b129ad7fd04163b47b7251aec1fe8de1182aa9e153cd9e85b51818c',
            'game_sha256': 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6',
            'fallthrough_rva': 0x3c8dc0, 'branch_rva': 0x3c8e78})
        self.assertEqual(row['body_guard'], {
            'rva': 0x3c8d50, 'length': 816,
            'sha256': '7c523b7c20db0d6c39ec594be62865fd48ef1b6891e4956ac692e4c2b019ca12'})
        entry = [row for row in self.targets if row['rva'] == 0x3c8d50]
        self.assertEqual(len(entry), 1)
        self.assertEqual(entry[0]['name'], 'mp.interventions.npc-entity-update.3c8d50')

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

    def test_gm_window_command_is_an_exclusive_pinned_entry(self):
        matches = [row for row in self.targets if row['rva'] == 0x6ea5a0]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.management-gm-window-command.6ea5a0')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['body_guard']['length'], 624)
        self.assertEqual(row['body_guard']['sha256'], 'e0ea25a0b830cd94eb7f01163ffc4993ade480f97c4e91d7f78650dad7c73355')
        self.assertEqual(row['code_guard']['length'], 30)
        self.assertEqual(row['code_guard']['bytes'], '5756534883ec404889ce4889d14889d3e8cb322800488378080474744889')


    def test_character_preview_uses_one_framework_rng_owner_and_exclusive_weapon_scope(self):
        expected = [{'name': 'framework.character.preview-get-int.9fc370', 'rva': 10470256, 'shared': True, 'groups': [{'feature': 'Character preview RNG source selection', 'phase': 'Framework GetInt owner, typed Service V2 registration', 'site': 'Framework-owned existing GetInt wrapper; typed selector only, no consumer hook or observer handoff.'}], 'code_guard': {'offset': 0, 'length': 30, 'bytes': '488b41084c8b41184c8b094c8b51104931c04989c34d31ca4d31c149c1c8', 'sha256': '75e9849e8f684c36d5dc0226a16ff77626e56f7727fe4784c50a29a03df827b6'}, 'body_guard': {'rva': 10470256, 'length': 96, 'sha256': '7041b7ec325591b8d191a54b3ac8d9bd909ee099283d9d210014af498d160c61'}}, {'name': 'mp.character.preview-attach-weapon.6577d0', 'rva': 6649808, 'shared': False, 'groups': [{'feature': 'Character weapon preview scope', 'phase': 'typed_action_runtime::start, bound Mainthread, once per process', 'site': 'Original ActorOrbitPane AttachWeapon preview; the Multiplayer owner validates exact receiver and TLS scope.'}], 'code_guard': {'offset': 0, 'length': 30, 'bytes': '4157415641554154555756534881ecc8000000488b81000700004889cb48', 'sha256': 'e392c726dbbb98eba244ab0a04153ee131cfd3449c54a2a46638cdb6b2010e51'}, 'body_guard': {'rva': 6649808, 'length': 1056, 'sha256': '2bee69e1eaea954f0f772cc46dce5d42eea03606285d951b0062c39aa3a15126'}}]
        for original in expected:
            rows = [row for row in self.targets if row['rva'] == original['rva']]
            self.assertEqual(rows, [original])
        self.assertNotIn('mp.character.preview-get-int.9fc370', [r['name'] for r in self.targets])

    def test_ability_apply_has_two_exclusive_pinned_presentation_entries(self):
        expected = [{'name': 'mp.ability-apply-presentation-setup.570900', 'rva': 5703936, 'shared': False, 'groups': [{'feature': 'new Apply SetupFor presentation boundary', 'phase': 'Ability enable, bound Character Mainthread, once per process', 'site': 'src/game/ability_apply_native.inc:49'}], 'code_guard': {'offset': 0, 'length': 6, 'bytes': '56534883ec28', 'sha256': 'e4d2e640858192b8785473b169480665e72378bda382e5b96fdf3317f6bce1bf', 'game_sha256': 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6', 'resume_rva': 5703942}, 'body_guard': {'rva': 5703936, 'length': 378, 'sha256': '8c17fdc046a0217cfb8e02cd8e6457c8a78e866f9fb3dba2340283a185f59054'}}, {'name': 'mp.ability-apply-presentation-event.5721f0', 'rva': 5710320, 'shared': False, 'groups': [{'feature': 'new Apply own Event40 presentation boundary', 'phase': 'Ability enable, bound Character Mainthread, once per process', 'site': 'src/game/ability_apply_native.inc:49'}], 'code_guard': {'offset': 0, 'length': 8, 'bytes': '555756534883ec28', 'sha256': '4692b8a69cc5e88c81eb4e467223072a5f99ba5e40d42d73c8bc37d7499ff4b5', 'game_sha256': 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6', 'resume_rva': 5710328}, 'body_guard': {'rva': 5710320, 'length': 419, 'sha256': 'ddd2f2414946173bed99d51289c42f3c0206168bef220d93646381599a351d87'}}]
        for original in expected:
            self.assertEqual([row for row in self.targets if row['rva'] == original['rva']], [original])
            guard = original['code_guard']
            self.assertEqual(guard['resume_rva'], original['rva'] + guard['length'])
            self.assertFalse(original['shared'])

    def test_ability_proposal_destroy_has_one_exclusive_pinned_entry(self):
        expected = {'name': 'mp.ability-proposal-window-destroy.569550', 'rva': 5674320, 'shared': False, 'groups': [{'feature': 'Preserve live Ability UID ownership while destroying the stock copied editor proposal', 'phase': 'Ability enable, bound main thread', 'site': 'src/game/ability_apply_native.inc'}], 'code_guard': {'offset': 0, 'length': 5, 'bytes': '534883ec20', 'sha256': '56d7f101a6c2ee7dc50760d9d7f16cfbbe7a7afb148ed0bfd4f643ecf9b77cee', 'game_sha256': 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6', 'resume_rva': 5674325}, 'body_guard': {'rva': 5674320, 'length': 213, 'sha256': '834bbdbafdc393bc2e64b7c5a4064aff0f2aea5f34987aea3a3a0f96f9a1be63'}}
        self.assertEqual([row for row in self.targets if row['rva'] == expected['rva']], [expected])
        self.assertEqual(expected['code_guard']['resume_rva'], expected['rva'] + expected['code_guard']['length'])

if __name__ == '__main__':
    unittest.main()
