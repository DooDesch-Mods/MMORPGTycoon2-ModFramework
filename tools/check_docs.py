"""Check built local links, images and offline script boundaries without external requests."""
from pathlib import Path
from html.parser import HTMLParser
from urllib.parse import urlsplit,unquote
import argparse,json,sys
ROOT=Path(__file__).resolve().parents[1]
class Links(HTMLParser):
    def __init__(self):super().__init__();self.links=[];self.ids=set();self.module=False
    def handle_starttag(self,tag,attrs):
        attrs=dict(attrs)
        if 'id' in attrs:self.ids.add(attrs['id'])
        for key in ('href','src','action'):
            if key in attrs:self.links.append(attrs[key])
        if tag=='script' and attrs.get('type')=='module':self.module=True
def check(root,offline):
    pages={path:Links() for path in root.rglob('*.html')};errors=[];links=0
    for path,parser in pages.items():parser.feed(path.read_text(encoding='utf-8'))
    for path,parser in pages.items():
        if offline and parser.module:errors.append(str(path)+': offline module script retained')
        for href in parser.links:
            url=urlsplit(href)
            if url.scheme or url.netloc:continue
            target=(root/url.path.lstrip('/')) if url.path.startswith('/') else path.parent/url.path
            target=target.resolve() if url.path else path
            if target.is_dir():target=target/'index.html'
            if not target.is_relative_to(root.resolve()):errors.append(str(path)+': link escapes documentation '+href);continue
            if not target.exists():errors.append(str(path)+': missing target '+href);continue
            if url.fragment and target in pages and unquote(url.fragment) not in pages[target].ids:errors.append(str(path)+': missing fragment '+href)
            links+=1
    if errors:raise RuntimeError('\n'.join(errors))
    return {'pages':len(pages),'local_links':links,'offline':offline}
def main():
    rows=[check(ROOT/'dist/docs/site',False),check(ROOT/'dist/docs/offline',True)]
    if not all(row['pages']>35 for row in rows):raise RuntimeError('Documentation outputs are incomplete')
    (ROOT/'artifacts/docs-links.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    print('info: built site/offline links and assets checked: '+json.dumps(rows))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError) as error:print('error: '+str(error),file=sys.stderr);sys.exit(1)
