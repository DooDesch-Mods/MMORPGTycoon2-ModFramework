import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from mt2fw import install, packages


class InstallTests(unittest.TestCase):
    def setUp(self):
        evidence = packages.FRAMEWORK / 'artifacts/install-tests'
        evidence.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(dir=evidence))
        self.game, self.package, self.profile = [self.root / p for p in ('game','package','profile')]
        self.game.mkdir(); self.package.mkdir()
        (self.game / 'MT2.exe').write_bytes(b'fixture-executable')
        self.patch = patch.object(install, 'EXE_SHA256', install.sha(self.game / 'MT2.exe'))
        self.patch.start(); self.addCleanup(self.patch.stop)
        for name in ('WSOCK32.dll','MT2Framework.dll'):
            (self.package / name).write_bytes(name.encode())
        self.manifest = {'version':'0.1.0','files':{p.name:install.sha(p) for p in self.package.iterdir()}}
        packages.atomic_json(self.package / 'framework-package.json', self.manifest)

    def test_install_update_remove_preserves_profile_and_originals(self):
        install.install(self.package,self.game,self.profile)
        (self.profile / 'precious-save.vrz').write_bytes(b'owned-data')
        old = (self.game/'MT2Framework.dll').read_bytes()
        (self.package/'MT2Framework.dll').write_bytes(b'updated-framework')
        self.manifest['files']['MT2Framework.dll']=install.sha(self.package/'MT2Framework.dll')
        packages.atomic_json(self.package/'framework-package.json',self.manifest)
        receipt = install.install(self.package,self.game,self.profile)
        self.assertEqual((Path(receipt['backup'])/'MT2Framework.dll').read_bytes(),old)
        install.uninstall(self.game)
        self.assertEqual((self.profile/'precious-save.vrz').read_bytes(),b'owned-data')
        self.assertEqual((self.game/'MT2.exe').read_bytes(),b'fixture-executable')
        self.assertFalse((self.game/'WSOCK32.dll').exists())

    def test_unknown_modified_and_outside_files_are_preserved(self):
        (self.game/'WSOCK32.dll').write_bytes(b'other-loader')
        with self.assertRaises(packages.PackageError):install.install(self.package,self.game,self.profile)
        self.assertEqual((self.game/'WSOCK32.dll').read_bytes(),b'other-loader')
        self.manifest['files']['MT2.exe']=install.sha(self.game/'MT2.exe')
        packages.atomic_json(self.package/'framework-package.json',self.manifest)
        with self.assertRaises(packages.PackageError):install.install(self.package,self.game,self.profile)

    def test_failed_receipt_commit_restores_first_install(self):
        original=install.atomic_json
        def fail(path,value):
            if path.name==install.RECEIPT:raise OSError('injected receipt failure')
            original(path,value)
        with patch.object(install,'atomic_json',side_effect=fail),self.assertRaises(OSError):
            install.install(self.package,self.game,self.profile)
        for name in ('WSOCK32.dll','MT2Framework.dll','mt2fw.json',install.RECEIPT):self.assertFalse((self.game/name).exists())

    def test_save_copy_waits_for_account_and_never_changes_source(self):
        source=self.root/'source.vrz';source.write_bytes(b'save-source')
        rules=self.root/'rules.vrt';rules.write_bytes(b'custom-world-rules')
        result=install.import_save(source,self.profile,'Copied World')
        self.assertEqual(result['state'],'pending')
        self.assertEqual(source.read_bytes(),b'save-source')
        account=self.profile/'game-data/76561198000000001';account.mkdir(parents=True)
        result=install.import_save(source,self.profile,'Copied World')
        self.assertEqual(result['state'],'complete')
        self.assertEqual((account/'save/Copied World/source.vrz').read_bytes(),source.read_bytes())
        self.assertEqual((account/'save/Copied World/rules.vrt').read_bytes(),rules.read_bytes())
        self.assertEqual(rules.read_bytes(),b'custom-world-rules')
        with self.assertRaises(packages.PackageError):install.import_save(source,self.profile,'Copied World')

    def test_classic_account_mods_and_loose_assets_are_detected_without_conversion(self):
        legacy=self.profile/'game-data/76561198000000001/mod/Legacy Pack';legacy.mkdir(parents=True)
        loose=self.game/'Data/Loose Pack';loose.mkdir(parents=True)
        found=install.classic_mods(self.game,self.profile)
        self.assertTrue(any(row['path']==str(legacy) and not row['managed'] for row in found))
        self.assertTrue(any(row['path']==str(loose) and row['kind']=='loose assets' for row in found))
        self.assertFalse((self.profile/'mods/Legacy Pack').exists())


if __name__=='__main__':unittest.main()
