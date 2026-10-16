"""Verify pinned native inputs and retain original notices for installed docs dependencies."""
from pathlib import Path
import argparse,hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1]
NATIVE=[('Lua','5.4.9','MIT','https://www.lua.org/ftp/lua-5.4.9.tar.gz','third_party/lua','README'),
 ('Dear ImGui','1.91.8','MIT','https://github.com/ocornut/imgui','third_party/vendor/imgui','LICENSE.txt'),
 ('MinHook','1.3.3','BSD-2-Clause','https://github.com/TsudaKageyu/minhook','third_party/vendor/minhook','LICENSE.txt'),
 ('nlohmann/json','vendored snapshot','MIT','https://github.com/nlohmann/json','third_party/vendor/nlohmann','LICENSE.MIT'),
 ('SDL3 SDK','3.2.24','Zlib','https://github.com/libsdl-org/SDL','third_party/vendor/SDL3','LICENSE.txt')]
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--record',action='store_true');args=parser.parse_args()
    rows=[]
    for name,version,license,url,folder,notice in NATIVE:
        tree=ROOT/folder
        if not (tree/notice).is_file():raise ValueError('Missing original dependency notice: '+name)
        rows.append({'name':name,'version':version,'license':license,'source':url,'notice':folder+'/'+notice,
                     'files':{p.relative_to(ROOT).as_posix():digest(p) for p in sorted(tree.rglob('*')) if p.is_file()}})
    native={'schema_version':1,'dependencies':rows,'lua_archive_sha256':'2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6','scope':'Vendored source and SDL import SDK; no original game DLL.'}
    path=ROOT/'licenses/dependencies.json'
    if args.record:path.write_text(json.dumps(native,indent=2)+'\n',encoding='utf-8')
    elif not path.is_file() or json.loads(path.read_text())!=native:raise ValueError('Vendored inputs differ; inspect before explicitly recording new inputs')
    lock=json.loads((ROOT/'package-lock.json').read_text(encoding='utf-8'));rows=[];missing=[];skipped=0
    notices=['# Documentation dependency notices','','Original license texts retained with web/offline output.','']
    for name,item in lock['packages'].items():
        if not name:continue
        folder=ROOT/name
        if not folder.is_dir():skipped+=1;continue
        package=json.loads((folder/'package.json').read_text(encoding='utf-8'));license=package.get('license',item.get('license'))
        if not license:raise ValueError('Missing license metadata: '+name)
        texts=[p for p in sorted(folder.iterdir()) if p.is_file() and p.name.upper().startswith(('LICENSE','LICENCE','COPYING','NOTICE'))]
        # Published subpackages can omit the shared repository notice; retain the inspected upstream text.
        if not texts:
            package_name=package.get('name','')
            if package_name.startswith('@algolia/autocomplete-'):
                if package['version']!='1.17.7':raise ValueError('Review changed autocomplete license input: '+name)
                texts=[ROOT/'licenses/upstream/autocomplete-1.17.7.txt']
            elif package_name.startswith('@docsearch/'):
                if package['version']!='3.8.2':raise ValueError('Review changed docsearch license input: '+name)
                texts=[ROOT/'licenses/upstream/docsearch-3.8.2.txt']
            elif package_name.startswith('@esbuild/'):texts=[ROOT/'node_modules/esbuild/LICENSE.md']
            elif package_name.startswith('@rollup/rollup-'):texts=[ROOT/'node_modules/rollup/LICENSE.md']
            elif package_name=='@iconify-json/simple-icons':texts=[ROOT/'licenses/upstream/CC0-1.0.txt']
            if any(not p.is_file() for p in texts):raise ValueError('Inspected upstream notice missing: '+name)
        if not texts:missing.append(name)
        row={'name':package.get('name',name),'version':package['version'],'license':license,'integrity':item.get('integrity'),'source':item.get('resolved'),'notices':[p.name for p in texts]};rows.append(row)
        notices+=['## '+row['name']+' '+row['version'],'','License: '+str(license),'']
        for notice in texts:notices+=['```text',notice.read_text(encoding='utf-8',errors='replace').strip(),'```','']
    if missing:raise ValueError('Packages without a standalone notice need inspection: '+', '.join(missing))
    (ROOT/'licenses/docs-dependencies.json').write_text(json.dumps({'packages':rows,'optional_platform_packages_not_installed':skipped},indent=2)+'\n',encoding='utf-8')
    (ROOT/'licenses/DOCS_DEPENDENCY_NOTICES.md').write_text('\n'.join(notices),encoding='utf-8')
    print(f'info: verified {len(native["dependencies"])} native trees; retained {len(rows)} docs package notices; {skipped} other-platform optional packages not installed')
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError) as error:print('error: '+str(error),file=sys.stderr);sys.exit(1)
