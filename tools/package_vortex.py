"""Package the original Vortex extension without a nested archive root."""
from pathlib import Path
import hashlib,json,zipfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'integrations/vortex/extension'
info=json.loads((source/'info.json').read_text(encoding='utf-8'))
if not isinstance(info['gameId'],int):raise RuntimeError('Nexus gameId must be numeric')
art=source/'gameart.png'
with Image.open(art) as image:
    if image.size!=(640,360):raise RuntimeError('Vortex game art must be 640x360')
if art.stat().st_size>1024*1024:raise RuntimeError('Vortex game art exceeds 1 MB')
output=ROOT/'dist/vortex';output.mkdir(parents=True,exist_ok=True)
archive=output/f'MMORPGTycoon2-Vortex-{info["version"]}.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as stream:
    for name in ('index.js','routing.js','guarded-deployment.js','steam-account.js','info.json','gameart.png','package.json'):
        stream.write(source/name,name)
    stream.write(ROOT/'LICENSE','LICENSE')
    stream.write(ROOT/'integrations/vortex/README.md','README.md')
with zipfile.ZipFile(archive) as stream:
    if stream.testzip():raise RuntimeError('Extension ZIP CRC failed')
report={'archive':str(archive),'version':info['version'],'gameId':info['gameId'],'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(ROOT/'artifacts').mkdir(exist_ok=True)
(ROOT/'artifacts/vortex-package.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
