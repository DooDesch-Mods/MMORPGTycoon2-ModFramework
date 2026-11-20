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
        matches = [row for row in self.targets if row['rva'] == 0x5c5b60]
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]['name'], 'mp.monster-release-capture.5bf3c0')
        self.assertIs(matches[0]['shared'], False)
        self.assertEqual(matches[0]['groups'][0]['feature'], 'Monster Release capture')
        self.assertEqual(matches[0]['groups'][0]['phase'], 'typed_action_runtime::start, bound Mainthread, once per process')


    def test_ordered_original_observers_have_exclusive_named_routes(self):
        for rva, name in [(0x540750, 'mp.ordered-tutorial-undo-clear.53a240'),
                          (0x560570, 'mp.ordered-clearance-make-scenery.559ee0')]:
            matches = [row for row in self.targets if row['rva'] == rva]
            self.assertEqual(len(matches), 1)
            self.assertEqual(matches[0]['name'], name)
            self.assertIs(matches[0]['shared'], False)

    def test_npc_camera_gate_preserves_one_exact_exclusive_original_branch(self):
        matches = [row for row in self.targets if row['rva'] == 0x3cd8c7]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.npc-camera-gate.3c8db7')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['code_guard'], {
            'offset': 0, 'length': 9, 'bytes': '8038000f85b8000000',
            'sha256': '9be443f68b129ad7fd04163b47b7251aec1fe8de1182aa9e153cd9e85b51818c',
            'game_sha256': '7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb',
            'fallthrough_rva': 0x3cd8d0, 'branch_rva': 0x3cd988})
        self.assertEqual(row['body_guard'], {
            'rva': 0x3cd860, 'length': 816,
            'sha256': '66305f8db1b8ab372fba5fb7f2ea5bff995d4cd0a8c29b25223e583f200ea0e5'})
        entry = [row for row in self.targets if row['rva'] == 0x3cd860]
        self.assertEqual(len(entry), 1)
        self.assertEqual(entry[0]['name'], 'mp.interventions.npc-entity-update.3c8d50')

    def test_toon_camera_gate_is_one_exact_exclusive_internal_site(self):
        matches = [row for row in self.targets if row['rva'] == 0x3eef72]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.toon-camera-schedule-gate.3ea462')
        self.assertIs(row['shared'], False)
        guard = row['code_guard']
        self.assertEqual(guard['bytes'], '80bbc0010000000f847303000080bb08080000000f84660300000f1f4000')
        self.assertEqual(guard['length'], 30)
        self.assertEqual(guard['sha256'], '3d45300ee4bf4b417a337091479e37bfd119ac92bc240759c02710e2b7d0d99f')
        self.assertEqual(guard['deferred_rva'], 0x3ef2f2)
        self.assertEqual(guard['full_rva'], 0x3eef90)

    def test_travel_rotation_gate_preserves_the_exact_original_branch(self):
        matches = [row for row in self.targets if row['rva'] == 0x3bc647]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.toon-travel-rotation-gate.3b7b37')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['code_guard']['bytes'], '0f8403060000')
        self.assertEqual(row['code_guard']['fallthrough_rva'], 0x3bc64d)
        self.assertEqual(row['code_guard']['branch_rva'], 0x3bcc50)
        self.assertEqual(row['body_guard']['rva'], 0x3bc3c0)
        self.assertEqual(row['body_guard']['length'], 5120)
        self.assertEqual(row['body_guard']['sha256'], '808046cf7663339aa9d0ecb2bc14b82f3cd853427a69b67c8cf75f484ba31ef2')

    def test_attribute_cadence_gate_preserves_the_exact_original_branch(self):
        matches = [row for row in self.targets if row['rva'] == 0x3a5442]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.toon-attribute-cadence-gate.3a0942')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['code_guard']['bytes'], '0f85380b0000')
        self.assertEqual(row['code_guard']['fallthrough_rva'], 0x3a5448)
        self.assertEqual(row['code_guard']['branch_rva'], 0x3a5f80)
        self.assertEqual(row['window_guard']['length'], 30)
        self.assertEqual(row['window_guard']['sha256'], 'daf271c95120774c9f0da8cf3696e87a1ce61f7053aa60371753fe406104e14b')
        self.assertEqual(row['body_guard']['rva'], 0x3a4e50)
        self.assertEqual(row['body_guard']['length'], 6816)
        self.assertEqual(row['body_guard']['sha256'], '2c83236299d0f9f98cda64d612d4ddf02d3aab8b8543e25b6744c2628e56cfc5')

    def test_gm_window_command_is_an_exclusive_pinned_entry(self):
        matches = [row for row in self.targets if row['rva'] == 0x6f1a10]
        self.assertEqual(len(matches), 1)
        row = matches[0]
        self.assertEqual(row['name'], 'mp.management-gm-window-command.6ea5a0')
        self.assertIs(row['shared'], False)
        self.assertEqual(row['body_guard']['length'], 624)
        self.assertEqual(row['body_guard']['sha256'], '200b4dc37700b313069f52dc1541dceb36f34daf6808804ea8cc5764b3bbcec6')
        self.assertEqual(row['code_guard']['length'], 30)
        self.assertEqual(row['code_guard']['bytes'], '5756534883ec404889ce4889d14889d3e87b502800488378080474744889')


    def test_character_preview_uses_one_framework_rng_owner_and_exclusive_weapon_scope(self):
        expected = [{'name': 'framework.character.preview-get-int.9fc370', 'rva': 10512544, 'shared': True, 'groups': [{'feature': 'Character preview RNG source selection', 'phase': 'Framework GetInt owner, typed Service V2 registration', 'site': 'Framework-owned existing GetInt wrapper; typed selector only, no consumer hook or observer handoff.'}], 'code_guard': {'offset': 0, 'length': 30, 'bytes': '488b41084c8b41184c8b094c8b51104931c04989c34d31ca4d31c149c1c8', 'sha256': '75e9849e8f684c36d5dc0226a16ff77626e56f7727fe4784c50a29a03df827b6'}, 'body_guard': {'rva': 10512544, 'length': 96, 'sha256': '7041b7ec325591b8d191a54b3ac8d9bd909ee099283d9d210014af498d160c61'}}, {'name': 'mp.character.preview-attach-weapon.6577d0', 'rva': 6678304, 'shared': False, 'groups': [{'feature': 'Character weapon preview scope', 'phase': 'typed_action_runtime::start, bound Mainthread, once per process', 'site': 'Original ActorOrbitPane AttachWeapon preview; the Multiplayer owner validates exact receiver and TLS scope.'}], 'code_guard': {'offset': 0, 'length': 30, 'bytes': '4157415641554154555756534881ecc8000000488b81000700004889cb48', 'sha256': 'e392c726dbbb98eba244ab0a04153ee131cfd3449c54a2a46638cdb6b2010e51'}, 'body_guard': {'rva': 6678304, 'length': 1056, 'sha256': '2bee69e1eaea954f0f772cc46dce5d42eea03606285d951b0062c39aa3a15126'}}]
        for original in expected:
            rows = [row for row in self.targets if row['rva'] == original['rva']]
            self.assertEqual(rows, [original])
        self.assertNotIn('mp.character.preview-get-int.9fc370', [r['name'] for r in self.targets])

    def test_ability_apply_has_two_exclusive_pinned_presentation_entries(self):
        expected = [{'name': 'mp.ability-apply-presentation-setup.570900', 'rva': 5730192, 'shared': False, 'groups': [{'feature': 'new Apply SetupFor presentation boundary', 'phase': 'Ability enable, bound Character Mainthread, once per process', 'site': 'src/game/ability_apply_native.inc:49'}], 'code_guard': {'offset': 0, 'length': 6, 'bytes': '56534883ec28', 'sha256': 'e4d2e640858192b8785473b169480665e72378bda382e5b96fdf3317f6bce1bf', 'game_sha256': '7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb', 'resume_rva': 5730198}, 'body_guard': {'rva': 5730192, 'length': 378, 'sha256': 'eb903b62213a53926dfaaf016da0a0324261a1481c87ade53227f025b5235663'}}, {'name': 'mp.ability-apply-presentation-event.5721f0', 'rva': 5736576, 'shared': False, 'groups': [{'feature': 'new Apply own Event40 presentation boundary', 'phase': 'Ability enable, bound Character Mainthread, once per process', 'site': 'src/game/ability_apply_native.inc:49'}], 'code_guard': {'offset': 0, 'length': 8, 'bytes': '555756534883ec28', 'sha256': '4692b8a69cc5e88c81eb4e467223072a5f99ba5e40d42d73c8bc37d7499ff4b5', 'game_sha256': '7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb', 'resume_rva': 5736584}, 'body_guard': {'rva': 5736576, 'length': 419, 'sha256': 'd9c3a9355df60fa34df0f42597f1eb7e9a7e7d3cbbdc1b7bea6fc1e376d4cbc7'}}]
        for original in expected:
            self.assertEqual([row for row in self.targets if row['rva'] == original['rva']], [original])
            guard = original['code_guard']
            self.assertEqual(guard['resume_rva'], original['rva'] + guard['length'])
            self.assertFalse(original['shared'])

    def test_ability_proposal_destroy_has_one_exclusive_pinned_entry(self):
        expected = {'name': 'mp.ability-proposal-window-destroy.569550', 'rva': 5700576, 'shared': False, 'groups': [{'feature': 'Preserve live Ability UID ownership while destroying the stock copied editor proposal', 'phase': 'Ability enable, bound main thread', 'site': 'src/game/ability_apply_native.inc'}], 'code_guard': {'offset': 0, 'length': 5, 'bytes': '534883ec20', 'sha256': '56d7f101a6c2ee7dc50760d9d7f16cfbbe7a7afb148ed0bfd4f643ecf9b77cee', 'game_sha256': '7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb', 'resume_rva': 5700581}, 'body_guard': {'rva': 5700576, 'length': 213, 'sha256': '7dfb74c4160e5c17933a8f0d8b1e76dfb5562d71268c7aa3531821f10825fe2f'}}
        self.assertEqual([row for row in self.targets if row['rva'] == expected['rva']], [expected])
        self.assertEqual(expected['code_guard']['resume_rva'], expected['rva'] + expected['code_guard']['length'])

if __name__ == '__main__':
    unittest.main()
