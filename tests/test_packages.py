import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from mt2fw import packages


class PackageTests(unittest.TestCase):
    def test_api_minor_version_boundary(self):
        value={'id':'test.api','name':'API test','version':'1.0.0','api_version':'1.1.0','type':'lua','entry':'main.lua','capabilities':[]}
        self.assertEqual(packages.validate_manifest(value)['api_version'],'1.1.0')
        value['api_version']='1.2.0'
        self.assertEqual(packages.validate_manifest(value)['api_version'],'1.2.0')
        value['api_version']='1.3.0'
        with self.assertRaises(packages.PackageError):packages.validate_manifest(value)
    def test_new_project_includes_current_agent_contract_and_license(self):
        mod=self.root/'agent-project';packages.new_mod(mod,'test.agent')
        self.assertEqual(packages.read_json(mod/'.mt2fw/api.json'),packages.read_json(packages.FRAMEWORK/'api/contract.json'))
        self.assertEqual((mod/'LICENSE').read_bytes(),(packages.FRAMEWORK/'LICENSE').read_bytes())
        self.assertIn('confirmed',(mod/'AGENTS.md').read_text(encoding='utf-8'))
    def test_semver_precedence_and_build_metadata(self):
        chain=['1.0.0-alpha','1.0.0-alpha.1','1.0.0-alpha.beta','1.0.0-beta','1.0.0-beta.2','1.0.0-beta.11','1.0.0-rc.1','1.0.0']
        self.assertEqual(sorted(chain,key=packages.version),chain)
        self.assertTrue(packages.matches('1.0.0+build','1.0.0+other'))
        self.assertFalse(packages.matches('1.0.0-rc.1','>=1.0.0'))
        for item in ('1.0.0-01','01.0.0','1.0.0-','1.0.0+'):
            with self.subTest(item=item),self.assertRaises(packages.PackageError):packages.version(item)
    def setUp(self):
        evidence = Path(__file__).resolve().parents[1] / 'artifacts/package-tests'
        evidence.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(dir=evidence))

    def test_template_pack_install_roundtrip(self):
        mod = self.root / 'hello'
        packages.new_mod(mod, 'test.hello')
        self.assertEqual(packages.check(mod)['manifest']['id'], 'test.hello')
        archive = self.root / 'hello.zip'
        packages.pack(mod, archive)
        installed = packages.install_mod(archive, self.root / 'profile')
        self.assertTrue(installed['restart_required'])
        self.assertEqual((Path(installed['directory']) / 'main.lua').read_bytes(), (mod / 'main.lua').read_bytes())
        again = packages.install_mod(archive, self.root / 'profile')
        self.assertTrue(Path(again['backup']).is_dir())

    def test_zip_escape_duplicate_case_and_symlink_are_rejected(self):
        for index, names in enumerate((['../escape'], ['A.lua', 'a.lua'], ['C:/escape'])):
            archive = self.root / f'bad-{index}.zip'
            with zipfile.ZipFile(archive, 'w') as stream:
                for name in names:
                    stream.writestr(name, 'bad')
            with self.subTest(names=names), self.assertRaises(packages.PackageError):
                packages.install_mod(archive, self.root / 'profile')
        archive = self.root / 'link.zip'
        with zipfile.ZipFile(archive, 'w') as stream:
            info = zipfile.ZipInfo('linked.lua')
            info.external_attr = 0o120777 << 16
            stream.writestr(info, '/outside')
        with self.assertRaises(packages.PackageError):
            packages.install_mod(archive, self.root / 'profile')

    def test_manifest_and_windows_path_contract(self):
        manifest = packages.read_json(packages.FRAMEWORK / 'templates/hello/mod.json')
        for key, value in [('id', 'Upper.Case'), ('entry', '../outside'), ('api_version', '2.0.0'), ('capabilities', ['objects.write'])]:
            with self.subTest(key=key), self.assertRaises(packages.PackageError):
                packages.validate_manifest({**manifest, key: value})
        for name in ('CON/file', 'file:stream', 'a/../b', 'a\\b', 'a./file', '/absolute'):
            with self.subTest(name=name), self.assertRaises(packages.PackageError):
                packages.relative(name)

    def test_new_never_overwrites_and_pack_is_outside_mod(self):
        mod = self.root / 'hello'
        packages.new_mod(mod, 'test.hello')
        with self.assertRaises(packages.PackageError):
            packages.new_mod(mod, 'test.hello')
        with self.assertRaises(packages.PackageError):
            packages.pack(mod, mod / 'recursive.zip')

    def test_two_scenery_templates_have_distinct_resource_names(self):
        left, right = self.root / 'left', self.root / 'right'
        packages.new_mod(left, 'author.left', 'scenery')
        packages.new_mod(right, 'author.right', 'scenery')
        a = packages.read_json(left / 'content.json')[0]
        b = packages.read_json(right / 'content.json')[0]
        self.assertNotEqual(a['variant'], b['variant'])
        self.assertNotEqual(a['model'], b['model'])
        self.assertEqual(packages.check(left)['manifest']['id'], 'author.left')


if __name__ == '__main__':
    unittest.main()
