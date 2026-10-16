import fnmatch
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
import hashlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mt2fw import packages
from package_runtime import RUNTIME_FILES,verify_runtime
import nexus_release

class RuntimePackageTests(unittest.TestCase):
    def setUp(self):
        folder=ROOT/'artifacts/runtime-package-tests';folder.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=folder);self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)

    def archive(self,kind='runtime',extra=None):
        path=self.root/'fixture.zip';files={name:b'fixture' for name in RUNTIME_FILES}
        if extra:files[extra]=b'developer fixture'
        metadata={'version':'0.1.1','kind':kind,'files':{name:hashlib.sha256(data).hexdigest() for name,data in files.items()}}
        with zipfile.ZipFile(path,'w') as stream:
            stream.writestr('framework-package.json',json.dumps(metadata))
            for name,data in files.items():stream.writestr(name,data)
        return path

    def test_only_runtime_with_complete_notices_can_pass_nexus_preflight(self):
        path=self.archive()
        self.assertEqual(nexus_release.archive_version(path,'framework'),'0.1.1')
        for name in ('mt2fw/MT2FW-Helper.exe','mt2fw/docs/quickstart.html','mt2fw/sdk/mt2.lua'):
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'player payload'):
                verify_runtime(self.archive(extra=name))
        with self.assertRaisesRegex(ValueError,'player runtime only'):verify_runtime(self.archive(kind='sdk'))

    def test_release_selector_excludes_sdk_even_when_both_are_on_github(self):
        pattern=nexus_release.targets()['framework']['asset_pattern']
        self.assertTrue(fnmatch.fnmatchcase('MT2Framework-0.1.1-windows-x64.zip',pattern))
        self.assertFalse(fnmatch.fnmatchcase('MT2Framework-SDK-0.1.1-windows-x64.zip',pattern))

    def test_native_validator_matches_shared_cli_and_does_not_execute_mods(self):
        checker=ROOT/'build/bin/Release/mt2fw.exe'
        self.assertTrue(checker.is_file(),'Build the native runtime checker first')
        base={'id':'test.package','name':'Fixture','version':'1.0.0','api_version':'1.0.0','type':'lua','entry':'main.lua','capabilities':['ui']}
        cases=[('valid',{},True),('local-write',{'multiplayer':'local','capabilities':['objects.write']},False),
               ('new-api',{'api_version':'2.0.0'},False),('missing-entry',{'entry':'missing.lua'},False),
               ('escape',{'entry':'../main.lua'},False),('reserved',{'entry':'NUL.txt'},False),
               ('bad-version',{'version':'1.0.0-01'},False),('bad-dependency',{'dependencies':{'other.mod':'garbage'}},False),
               ('bad-capability',{'capabilities':['unknown']},False),('wrong-extension',{'entry':'mod.json'},False)]
        for name,changes,accepted in cases:
            with self.subTest(name=name):
                folder=self.root/name;folder.mkdir()
                (folder/'mod.json').write_text(json.dumps({**base,**changes}),encoding='utf-8')
                (folder/'main.lua').write_text('error("must never execute during validation")',encoding='utf-8')
                try:expected=packages.check(folder);python_ok=True
                except (ValueError,OSError,KeyError):python_ok=False
                result=subprocess.run([str(checker),'check',str(folder)],capture_output=True,text=True,timeout=10)
                self.assertEqual(python_ok,accepted)
                self.assertEqual(result.returncode==0,accepted,result.stderr)
                if accepted:self.assertEqual(json.loads(result.stdout),expected)
                else:self.assertIn('error:',result.stderr)
        usage=subprocess.run([str(checker),'new','test.mod'],capture_output=True,text=True,timeout=10)
        self.assertEqual(usage.returncode,2)
        self.assertIn('SDK',usage.stderr)

    def test_native_validator_checks_content_files_and_unicode_paths(self):
        checker=ROOT/'build/bin/Release/mt2fw.exe'
        folder=self.root/'Grüße';folder.mkdir()
        manifest={'id':'test.scenery','name':'Scenery','version':'1.0.0','api_version':'1.0.0','type':'content','entry':'content.json','capabilities':['content']}
        (folder/'mod.json').write_text(json.dumps(manifest),encoding='utf-8')
        (folder/'model.vmb').write_bytes(b'fixture');(folder/'material.vmt').write_bytes(b'fixture')
        definition={'id':'test.scenery:prop','model':'model.vmb','material':'material.vmt'}
        for definitions,accepted in [([definition],True),([definition,definition],False),([{**definition,'model':'missing.vmb'}],False),([],False)]:
            (folder/'content.json').write_text(json.dumps(definitions),encoding='utf-8')
            result=subprocess.run([str(checker),'check',str(folder)],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode==0,accepted,result.stderr)

if __name__=='__main__':unittest.main()
