"""Prepare a stable Nexus upload archive from the verified portable product."""
from pathlib import Path
import hashlib,json,shutil
from package_runtime import verify_runtime
ROOT=Path(__file__).resolve().parents[1]
delivery=json.loads((ROOT/'artifacts/product-delivery.json').read_text(encoding='utf-8'))
source=Path(delivery['archive'])
manifest=verify_runtime(source)
output=ROOT/'dist/nexus';output.mkdir(parents=True,exist_ok=True)
target=output/f'MT2Framework-{manifest["version"]}-windows-x64.zip'
shutil.copy2(source,target)
receipt={'archive':str(target),'version':manifest['version'],'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'files':len(manifest['files']),'format':'Player runtime only; Vortex routes the same archive without repacking','nexus_upload':False}
(ROOT/'artifacts/nexus-package.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(json.dumps(receipt,indent=2))
