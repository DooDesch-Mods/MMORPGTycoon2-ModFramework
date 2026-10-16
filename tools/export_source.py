"""Export only non-ignored local repository source; do not commit, push or create a remote."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,zipfile
ROOT=Path(__file__).resolve().parents[1]
def main():
    value=subprocess.run(['git','-C',str(ROOT),'ls-files','--cached','--others','--exclude-standard','-z'],capture_output=True,check=True)
    files=sorted(set(name for name in value.stdout.decode('utf-8').split('\0') if name))
    forbidden={'MT2.exe','steam_api64.dll','SDL3.dll','private-reference'}
    private_roots=[str(Path.home()).replace('\\','/').casefold(),str(ROOT.parent.parent).replace('\\','/').casefold()]
    for name in files:
        path=Path(name)
        if forbidden.intersection(path.parts) or path.suffix.lower() in ('.vrz','.exe','.dll','.pdb','.log'):raise RuntimeError('Private or runtime artifact in source export: '+name)
        if any(part in ('.git','node_modules','build','artifacts','dist','__pycache__') for part in path.parts):raise RuntimeError('Ignored directory unexpectedly included: '+name)
        if path.suffix.lower() in ('.py','.mjs','.mts','.ts','.vue','.md','.json','.yml','.cpp','.h','.txt'):
            text=(ROOT/path).read_text(encoding='utf-8-sig')
            normalized=text.replace('\\\\','/').replace('\\','/').casefold()
            if any(value in normalized for value in private_roots):raise RuntimeError('Machine-specific path in source: '+name)
    output=ROOT/'dist/source';output.mkdir(parents=True,exist_ok=True)
    archive=output/('MT2-ModFramework-0.1.3-source-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.zip')
    with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as stream:
        for name in files:stream.write(ROOT/name,name)
    with zipfile.ZipFile(archive) as stream:
        if stream.testzip():raise RuntimeError('Source ZIP CRC failed')
    remotes=subprocess.run(['git','-C',str(ROOT),'remote'],capture_output=True,text=True,check=True).stdout.strip()
    report={'archive':str(archive),'files':len(files),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'git_remote':bool(remotes),'commits_created_by_export':False,'publication_by_export':False}
    (ROOT/'artifacts').mkdir(exist_ok=True);(ROOT/'artifacts/source-export.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as error:print('error: '+str(error),file=sys.stderr);sys.exit(1)
