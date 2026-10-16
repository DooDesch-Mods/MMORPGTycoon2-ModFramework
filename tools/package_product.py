"""Assemble an allowlisted local product package; never include installed game data."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import importlib.metadata
import json
import shutil
import sys
import uuid
import re
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mt2fw import packages
from mt2fw.install import sha

destination=ROOT/'dist'/('MT2Framework-0.1.0-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
destination.mkdir(parents=True)

def copy(source,target):
    if not source.is_file():raise RuntimeError('Required product input missing: '+str(source))
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)

for name in ('WSOCK32.dll','MT2Framework.dll'):copy(ROOT/'build/bin/Release'/name,destination/name)
for name in ('mt2fw.exe','MT2FW-Helper.exe'):copy(ROOT/'dist/helper'/name,destination/'mt2fw'/name)
offline=ROOT/'dist/docs/offline'
if not (offline/'quickstart.html').is_file():raise RuntimeError('Build offline documentation first')
for source_root,folder in ((offline,'docs'),(ROOT/'sdk','sdk'),(ROOT/'include','include'),(ROOT/'templates','templates'),(ROOT/'examples','examples'),(ROOT/'api','api'),(ROOT/'assets','assets')):
    for source in sorted(source_root.rglob('*')):
        if folder=='include' and source.is_file() and source.relative_to(source_root).as_posix()!='mt2fw/plugin.h':continue
        if source.is_file() and not any(part in ('__pycache__','build') for part in source.relative_to(source_root).parts):copy(source,destination/'mt2fw'/folder/source.relative_to(source_root))
for name in ('LICENSE','README.md','THIRD_PARTY_NOTICES.md'):copy(ROOT/name,destination/'mt2fw'/name)
copy(ROOT/'integrations/vortex/README.md',destination/'mt2fw/integrations/vortex/README.md')
readme=destination/'mt2fw/README.md'
text=readme.read_text(encoding='utf-8').replace('docs/public/media/','docs/media/')
text=text.replace('https://doodesch-mods.github.io/MMORPGTycoon2-ModFramework/','docs/')
text=text.replace('[CONTRIBUTING.md](CONTRIBUTING.md)', '[the contributor guide](docs/contributing/build.md)')
text=re.sub(r'\]\((docs/[^)]+)\.md\)',r'](\1.html)',text)
readme.write_text(text,encoding='utf-8')
copy(ROOT/'evidence/runtime-acceptance.json',destination/'mt2fw/evidence/runtime-acceptance.json')
copy(ROOT/'evidence/repository-acceptance.json',destination/'mt2fw/evidence/repository-acceptance.json')
copy(ROOT/'evidence/vortex-acceptance.json',destination/'mt2fw/evidence/vortex-acceptance.json')
copy(ROOT/'evidence/vortex-scenery-acceptance.json',destination/'mt2fw/evidence/vortex-scenery-acceptance.json')
notices=destination/'mt2fw/licenses'
for folder,name in [('imgui','LICENSE.txt'),('minhook','LICENSE.txt'),('nlohmann','LICENSE.MIT'),('SDL3','LICENSE.txt')]:
    copy(ROOT/'third_party/vendor'/folder/name,notices/(folder+'-'+name))
copy(ROOT/'third_party/lua/README',notices/'lua-README.txt')
for source in (ROOT/'licenses').rglob('*'):
    if source.is_file():copy(source,notices/source.relative_to(ROOT/'licenses'))
for distribution in ('Pillow','PyInstaller','altgraph','packaging','pywin32-ctypes','pefile','pyinstaller-hooks-contrib'):
    found=importlib.metadata.distribution(distribution)
    for item in found.files or []:
        if 'license' in item.name.lower() or item.name=='COPYING.txt':
            if found.locate_file(item).is_file():copy(Path(found.locate_file(item)),notices/distribution/str(item).replace('..','_'))
base=Path(sys.base_prefix)
copy(base/'LICENSE.txt',notices/'Python-LICENSE.txt')
copy(ROOT/'licenses/Tcl-8.6.15-license.terms',notices/'Tcl-8.6.15-license.terms')
copy(ROOT/'licenses/sources.json',notices/'sources.json')
copy(base/'tcl/tk8.6/license.terms',notices/'Tk-license.terms')
files={p.relative_to(destination).as_posix():sha(p) for p in sorted(destination.rglob('*')) if p.is_file()}
packages.atomic_json(destination/'framework-package.json',{'version':'0.1.0','files':files,'channel':'development','acceptance':'See framework evidence; not a public release'})
packages.atomic_json(ROOT/'artifacts/product-candidate.json',{'directory':str(destination),'files':len(files),'manifest_sha256':sha(destination/'framework-package.json')})
archive=destination.parent/(destination.name+'.zip')
with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as stream:
    for source in sorted(destination.rglob('*')):
        if source.is_file():stream.write(source,source.relative_to(destination).as_posix())
with zipfile.ZipFile(archive) as stream:
    if stream.testzip():raise RuntimeError('Product ZIP CRC failed')
delivery={'directory':str(destination),'archive':str(archive),'files':len(files),'sha256':sha(archive),'local_only':True}
packages.atomic_json(ROOT/'artifacts/product-delivery.json',delivery)
print(json.dumps(delivery,indent=2))
