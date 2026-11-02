"""Apply the prepared summary and BBCode description to one verified Nexus release."""
from pathlib import Path
import argparse
import hashlib
import html
import json
import os
import re
import sys
import uuid

from nexus_release import API, ROOT, targets
from release_delivery import request_json

PRESERVED = ('name', 'author', 'uploader', 'game_id', 'game_scoped_id',
             'category', 'tags', 'primary_image', 'gallery_images')


def description(value):
    text = html.unescape(value or '').replace('\r\n', '\n')
    # Nexus stores each BBCode newline with an additional HTML line-break marker.
    return re.sub(r'\n<br\s*/?>', '\n', text, flags=re.I).strip()


def update_listing(directory, request=request_json):
    plan = json.loads((directory / 'plan.json').read_text(encoding='utf-8'))
    if plan['status'] not in ('already-present', 'ready'):
        raise ValueError('Listing update requires a verified published release plan')
    product = plan['product']
    target = targets()[product]
    source = ROOT / 'integrations/nexus/listings' / product
    cohort = json.loads((source / 'release.json').read_text(encoding='utf-8'))
    for field in ('tag', 'version', 'source_commit', 'sha256'):
        if cohort.get(field) != plan.get(field):
            raise ValueError('Prepared listing release differs: ' + field)
    key = os.environ.get('NEXUSMODS_API_KEY', '').strip()
    if not key:
        raise ValueError('NEXUSMODS_API_KEY is missing')
    headers = {'apikey': key}
    summary = re.sub(r'\s+', ' ', (source / 'summary.txt').read_text(encoding='utf-8')).strip()
    body = (source / 'description.bbcode.txt').read_text(encoding='utf-8').strip()
    if not summary or len(summary) > 350 or not body:
        raise ValueError('Prepared Nexus listing needs a summary of 1 to 350 characters and a description')
    url = API + '/mods/' + target['mod_uid']
    summary_url = f"https://api.nexusmods.com/v1/games/{target['domain']}/mods/{target['page_id']}.json"
    def read_summary():
        # Legacy mod responses can stay cached across a successful v3 metadata write.
        return request(summary_url + '?mt2fw_read=' + uuid.uuid4().hex,
                       headers={**headers, 'Cache-Control': 'no-cache, no-store'})
    before = request(url, headers=headers)['data']
    if (str(before['id']) != target['mod_uid'] or str(before['game_id']) != str(target['game_id'])
            or str(before['game_scoped_id']) != str(target['page_id']) or before['version'] != plan['version']):
        raise ValueError('Nexus listing identity/version differs from the release plan')
    before_summary = read_summary()
    if before_summary.get('version') != plan['version'] or str(before_summary.get('mod_id')) != str(target['page_id']):
        raise ValueError('Nexus summary identity/version differs from the release plan')
    current_summary = re.sub(r'\s+', ' ', before_summary.get('summary') or '').strip()
    changed = current_summary != summary or description(before.get('description')) != description(body)
    if changed:
        request(url, 'PATCH', {'summary': summary, 'description': body}, headers)
    after = request(url, headers=headers)['data']
    after_summary = read_summary()
    if (description(after.get('description')) != description(body)
            or re.sub(r'\s+', ' ', after_summary.get('summary') or '').strip() != summary):
        raise RuntimeError('Nexus listing acknowledgement differs from the prepared text; inspect before retrying')
    for field in ('id', 'version', *PRESERVED):
        if after.get(field) != before.get(field):
            raise RuntimeError('Nexus listing changed an unrelated field: ' + field)
    receipt = {'status': 'confirmed', 'product': product, 'version': plan['version'],
               'nexus_url': plan['nexus_url'], 'changed': changed,
               'fields': ['summary', 'description'], 'preserved_fields': list(PRESERVED),
               'summary_sha256': hashlib.sha256(summary.encode('utf-8')).hexdigest(),
               'description_sha256': hashlib.sha256(body.encode('utf-8')).hexdigest()}
    (directory / 'listing.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(receipt))
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    try:
        update_listing(parser.parse_args().directory)
    except (KeyError, ValueError, OSError, RuntimeError) as error:
        print('error: ' + str(error), file=sys.stderr)
        sys.exit(1)
