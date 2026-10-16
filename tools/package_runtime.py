"""Build and verify the player-only payload, excluding Python and developer tools."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,shutil,sys,zipfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mt2fw import packages
from mt2fw.install import sha

NATIVE_LICENSES={'imgui':'LICENSE.txt','minhook':'LICENSE.txt','nlohmann':'LICENSE.MIT','SDL3':'LICENSE.txt'}
RUNTIME_FILES={'WSOCK32.dll','MT2Framework.dll','mt2fw/mt2fw.exe','mt2fw/README.md','mt2fw/LICENSE','mt2fw/THIRD_PARTY_NOTICES.md',
               'mt2fw/licenses/lua-README.txt',*(f'mt2fw/licenses/{name}-{file}' for name,file in NATIVE_LICENSES.items())}

def copy(source,target):
    if not source.is_file():raise RuntimeError('Required runtime input missing: '+str(source))
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)

def verify_runtime(archive):
    with zipfile.ZipFile(archive) as stream:
        if stream.testzip():raise ValueError('Runtime ZIP CRC failed')
        manifest=json.loads(stream.read('framework-package.json'))
        if manifest.get('kind')!='runtime':raise ValueError('Nexus accepts the player runtime only')
        names=stream.namelist()
        if len(names)!=len(set(names)) or set(names)!=RUNTIME_FILES|{'framework-package.json'} or set(manifest['files'])!=RUNTIME_FILES:
            raise ValueError('Runtime must contain only the player payload and its notices')
        for name,expected in manifest['files'].items():
            if hashlib.sha256(stream.read(name)).hexdigest()!=expected:raise ValueError('Runtime file checksum differs: '+name)
    return manifest

def build(root,version):
    destination=root/'dist'/('runtime-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
    destination.mkdir(parents=True)
    for name in ('WSOCK32.dll','MT2Framework.dll'):copy(root/'build/bin/Release'/name,destination/name)
    copy(root/'build/bin/Release/mt2fw.exe',destination/'mt2fw/mt2fw.exe')
    copy(root/'packaging/runtime/README.md',destination/'mt2fw/README.md')
    copy(root/'packaging/runtime/THIRD_PARTY_NOTICES.md',destination/'mt2fw/THIRD_PARTY_NOTICES.md')
    copy(root/'LICENSE',destination/'mt2fw/LICENSE')
    for folder,name in NATIVE_LICENSES.items():copy(root/'third_party/vendor'/folder/name,destination/'mt2fw/licenses'/(folder+'-'+name))
    copy(root/'third_party/lua/README',destination/'mt2fw/licenses/lua-README.txt')
    files={p.relative_to(destination).as_posix():sha(p) for p in sorted(destination.rglob('*')) if p.is_file()}
    packages.atomic_json(destination/'framework-package.json',{'version':version,'kind':'runtime','files':files})
    archive=root/'dist/runtime'/f'MT2Framework-{version}-windows-x64.zip';archive.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as stream:
        for source in sorted(destination.rglob('*')):
            if source.is_file():stream.write(source,source.relative_to(destination).as_posix())
    verify_runtime(archive)
    receipt={'directory':str(destination),'archive':str(archive),'files':len(files),'sha256':sha(archive),'bytes':archive.stat().st_size,'kind':'runtime','version':version}
    packages.atomic_json(root/'artifacts/product-delivery.json',receipt)
    return destination
