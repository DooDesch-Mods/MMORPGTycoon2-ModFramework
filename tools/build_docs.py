"""Build the same authored/generated content for web, file-safe offline use and agents."""
from pathlib import Path
from datetime import datetime,timezone
import argparse,hashlib,html,json,os,re,shutil,subprocess,sys
from html.parser import HTMLParser
from urllib.parse import urlsplit,urlunsplit
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mt2fw import packages

def source_pages():return sorted(p for p in (ROOT/'docs').rglob('*.md') if '.vitepress' not in p.parts)
def prepare():
    result=subprocess.run([sys.executable,str(ROOT/'tools/generate_docs.py')],cwd=ROOT)
    if result.returncode:raise RuntimeError('API generation failed')
    result=subprocess.run([sys.executable,str(ROOT/'tools/license_inventory.py')],cwd=ROOT)
    if result.returncode:raise RuntimeError('Dependency notice/integrity check failed')
    media=packages.read_json(ROOT/'docs/media.json')
    captures=ROOT/'artifacts/helper-doc-captures.json'
    if captures.exists():
        for row in packages.read_json(captures):
            row.update(case=row['id'],alt='English helper '+row['id'].replace('-',' '),caption='Real owned English helper capture.')
            if row['id'] not in {item['id'] for item in media['images']}:media['images'].append(row)
    for image in media['images']:
        path=ROOT/'docs'/image['path']
        if not path.is_file():raise RuntimeError('Documented media file missing: '+str(path))
        for field in ('locale','origin','rights','framework_version','alt','caption'):
            if not image.get(field):raise RuntimeError('Media metadata missing '+field+': '+image['id'])
        image['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    packages.atomic_json(ROOT/'docs/media.json',media)
    pages=[]
    for path in source_pages():
        source=path.read_text(encoding='utf-8');match=re.search(r'^title:\s*(.+)$',source,re.M)
        title=match[1].strip().strip('"') if match else path.stem
        text=re.sub(r'<[^>]+>',' ',re.sub(r'^---\n.*?\n---\n','',source,flags=re.S))
        pages.append({'title':title,'href':path.relative_to(ROOT/'docs').with_suffix('.html').as_posix(),'text':re.sub(r'\s+',' ',text)})
    (ROOT/'docs/public/search-index.js').write_text('window.MT2FW_SEARCH_INDEX = '+json.dumps(pages,ensure_ascii=False).replace('</','<\\/')+';\n',encoding='utf-8')
    llms='# MT2 Mod Framework\n\n> Framework 0.1.0, Lua API 1.0.0, native ABI 1. Windows x64 Steam. Multiplayer compatibility is not established.\n\nUse the version matching the installed framework. World mutations need confirmed acknowledgement; never automatically retry unknown results.\n\n## Start\n\n- [First mod](quickstart.md): create, edit, reload and package Lua.\n- [Agent guide](agents/start.md): lifecycle rules and validation.\n- [API reference](reference/index.md): generated public functions.\n- [Events](reference/events.md): exact event payload types.\n- [Native C ABI](reference/native.md): authoritative header and ownership.\n- [Compatibility](compatibility.md): supported executable and current limits.\n\n## Machine contracts\n\n- [API JSON](api/1.0.0/api.json): functions, types, capabilities, errors and examples.\n- [Mod schema](api/1.0.0/mod.schema.json): structure; mt2fw check adds semantic validation.\n- [Versioned Markdown](versions/1.0.0/reference/index.md): documentation snapshot for this API.\n'
    (ROOT/'docs/public/llms.txt').write_text(llms,encoding='utf-8')
    (ROOT/'docs/public/media/LICENSE.txt').write_text('Framework UI and original branding: Copyright (c) 2026 DooDesch, MIT.\nOriginal game artwork shown in real MMORPG Tycoon 2 captures retains its respective rights and is not licensed under the framework MIT license.\nNo original game files or private reference icon are redistributed here. See media.json for individual capture cases.\n',encoding='utf-8')
    return pages

class OfflineHTML(HTMLParser):
    def __init__(self,page,root):super().__init__(convert_charrefs=False);self.page=page;self.root=root;self.parts=[]
    def tag(self,tag,attrs,closing=False):
        values=[]
        for key,value in attrs:
            if value is not None and key in ('href','src','action') and value.startswith('/') and not value.startswith('//'):
                url=urlsplit(value);target=self.root/url.path.lstrip('/')
                if not url.path.rsplit('/',1)[-1]:target=target/'index.html'
                value=urlunsplit(('', '',os.path.relpath(target,self.page.parent).replace('\\','/'),url.query,url.fragment))
            values.append(key if value is None else key+'="'+html.escape(value,quote=True)+'"')
        self.parts.append('<'+tag+(' '+' '.join(values) if values else '')+('/>' if closing else '>'))
    def handle_starttag(self,tag,attrs):self.tag(tag,attrs)
    def handle_startendtag(self,tag,attrs):self.tag(tag,attrs,True)
    def handle_endtag(self,tag):self.parts.append('</'+tag+'>')
    def handle_data(self,data):self.parts.append(data)
    def handle_entityref(self,name):self.parts.append('&'+name+';')
    def handle_charref(self,name):self.parts.append('&#'+name+';')
    def handle_comment(self,data):self.parts.append('<!--'+data+'-->')
    def handle_decl(self,data):self.parts.append('<!'+data+'>')

def export_docs(pages,site,offline,markdown):
    # GitHub source links include public/; VitePress copies that directory's contents to the site root.
    for path in site.rglob('*.html'):
        text=path.read_text(encoding='utf-8').replace('href="./public/media/','href="/media/')
        path.write_text(text,encoding='utf-8')
    shutil.copytree(site,offline,dirs_exist_ok=True)
    for path in source_pages():
        relative=path.relative_to(ROOT/'docs')
        text=re.sub(r'^---\n.*?\n---\n','',path.read_text(encoding='utf-8'),flags=re.S).replace('public/media/','media/')
        for base in (site,offline,markdown):
            for name in (relative,Path('versions/1.0.0')/relative):
                target=base/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text,encoding='utf-8')
    for base in (site,offline):
        shutil.copytree(ROOT/'sdk',base/'sdk',dirs_exist_ok=True)
        shutil.copy2(ROOT/'include/mt2fw/plugin.h',base/'sdk/plugin.h')
        shutil.copytree(ROOT/'examples',base/'examples',dirs_exist_ok=True,ignore=shutil.ignore_patterns('build','__pycache__'))
        shutil.copy2(ROOT/'LICENSE',base/'LICENSE')
        shutil.copy2(ROOT/'THIRD_PARTY_NOTICES.md',base/'THIRD_PARTY_NOTICES.md')
        (base/'licenses').mkdir(exist_ok=True)
        for name in ('DOCS_DEPENDENCY_NOTICES.md','docs-dependencies.json','dependencies.json'):
            shutil.copy2(ROOT/'licenses'/name,base/'licenses'/name)
        shutil.copy2(ROOT/'docs/media.json',base/'media.json')
    for path in offline.rglob('*.html'):
        source=path.read_text(encoding='utf-8')
        def strip_script(match):return match[0] if re.search(r'src=["\'][^"\']*(?:search-index|docs-ui)\.js["\']',match[0]) else ''
        source=re.sub(r'<script\b[^>]*>.*?</script>',strip_script,source,flags=re.S|re.I)
        source=re.sub(r'<link\b[^>]*rel=["\']modulepreload["\'][^>]*>','',source,flags=re.I)
        parser=OfflineHTML(path,offline);parser.feed(source);parser.close();path.write_text(''.join(parser.parts),encoding='utf-8')
    for base in (site,offline):
        for path in base.rglob('*.html'):
            relative=path.relative_to(base);md=relative.with_suffix('.md').name
            if (path.with_suffix('.md')).is_file():
                described=os.path.relpath(base/'llms.txt',path.parent).replace('\\','/')
                text=path.read_text(encoding='utf-8').replace('</head>','<link rel="alternate" type="text/markdown" href="'+md+'"><link rel="describedby" href="'+described+'"></head>')
                path.write_text(text,encoding='utf-8')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--prepare-only',action='store_true');parser.add_argument('--reuse-site',action='store_true');args=parser.parse_args()
    pages=prepare()
    if args.prepare_only:print('info: API, search index, media metadata and llms.txt prepared');return
    logdir=ROOT/'artifacts/docs-build'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');logdir.mkdir(parents=True)
    if not args.reuse_site:
        command=['npm.cmd' if os.name=='nt' else 'npm','run','docs:site']
        with (logdir/'vitepress.log').open('w',encoding='utf-8') as stream:result=subprocess.run(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
        if result.returncode:raise RuntimeError('VitePress build failed: '+str(logdir/'vitepress.log'))
    built=ROOT/'docs/.vitepress/dist'
    if not (built/'index.html').is_file():raise RuntimeError('Built documentation site is missing')
    output=ROOT/'dist/docs';site=output/'site';offline=output/'offline';markdown=output/'markdown'
    # Preserve previous outputs as complete snapshots instead of deleting computed trees.
    if output.exists():output.rename(logdir/'previous-output')
    shutil.copytree(built,site);export_docs(pages,site,offline,markdown)
    report={'pages':len(pages),'site':str(site),'offline':str(offline),'markdown':str(markdown),'api_version':'1.0.0'}
    packages.atomic_json(logdir/'result.json',report);packages.atomic_json(ROOT/'artifacts/docs-current.json',report)
    print('info: web/offline/Markdown docs built: '+str(output))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError) as error:print('error: '+str(error),file=sys.stderr);sys.exit(1)
