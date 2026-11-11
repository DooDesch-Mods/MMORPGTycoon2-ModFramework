"""Validate a release directory against its tag and Nexus destination, confirm uploads and complete changelogs.

The upload itself runs in the official Nexus upload action (.github/workflows/release.yml).
"""
from pathlib import Path, PurePosixPath
import argparse
import fnmatch
import hashlib
import html
import json
import os
import re
import sys
import urllib.error
import urllib.request
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from package_runtime import verify_runtime
from release_notes import section, plain, bbcode

API = 'https://api.nexusmods.com/v3'
TAG_PREFIXES = (('vortex-v', 'vortex'), ('v', 'framework'))


def targets():
    result = json.loads((ROOT / 'integrations/nexus/targets.json').read_text(encoding='utf-8'))
    for key, value in result.items():
        if int(value['mod_uid']) != (value['game_id'] << 32) + value['page_id']:
            raise ValueError(f'{key}: Unique Mod ID does not match game/page IDs')
        if not re.fullmatch(r'[1-9][0-9]*', value['file_id']):
            raise ValueError(f'{key}: invalid Nexus file ID')
    return result


def version_key(value):
    match = re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?', value)
    if not match:
        raise ValueError('Expected SemVer version: ' + value)
    suffix = match[4]
    parts = tuple((0, int(part)) if part.isdigit() else (1, part) for part in suffix.split('.')) if suffix else ()
    return tuple(int(match[i]) for i in (1, 2, 3)) + (suffix is None, parts)


def product_for(tag):
    """Return (product, version) for a release tag: v<version> is the framework, vortex-v<version> the extension."""
    for prefix, product in TAG_PREFIXES:
        if tag.startswith(prefix):
            version = tag[len(prefix):]
            version_key(version)
            return product, version
    raise ValueError('Release tags are v<version> or vortex-v<version>: ' + tag)


def archive_version(path, product):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate ZIP entries: ' + path.name)
        for name in names:
            normalized = PurePosixPath(name.replace('\\', '/'))
            if normalized.is_absolute() or '..' in normalized.parts or ':' in name:
                raise ValueError('Unsafe ZIP path: ' + name)
        if archive.testzip():
            raise ValueError('ZIP CRC failed: ' + path.name)
        if product == 'framework':
            metadata = json.loads(archive.read('framework-package.json'))
            if set(names) != set(metadata['files']) | {'framework-package.json'}:
                raise ValueError('Framework inventory differs')
            for name, expected in metadata['files'].items():
                if hashlib.sha256(archive.read(name)).hexdigest() != expected:
                    raise ValueError('Framework file checksum differs: ' + name)
            verify_runtime(path)
        elif product == 'vortex':
            metadata = json.loads(archive.read('info.json'))
            if metadata['gameId'] != 4452 or metadata['id'] != 'game-mmorpgtycoon2':
                raise ValueError('Wrong Vortex game extension')
        else:
            raise ValueError('Unknown release product: ' + product)
        version_key(metadata['version'])
        return metadata['version']


def checksum_for(text, filename):
    matches = []
    for line in text.splitlines():
        match = re.fullmatch(r'([a-fA-F0-9]{64})\s+\*?(.+)', line)
        if not match:
            raise ValueError('Malformed SHA256SUMS.txt line')
        if match[2] == filename:
            matches.append(match[1].lower())
    if len(matches) != 1:
        raise ValueError('Expected exactly one checksum for ' + filename)
    return matches[0]


def request_json(url, method='GET', body=None, headers=None):
    data = None if body is None else json.dumps(body).encode('utf-8')
    request = urllib.request.Request(url, data=data, method=method,
        headers={'Accept': 'application/json', 'Content-Type': 'application/json',
                 'User-Agent': 'MT2-ModFramework-CI', **(headers or {})})
    try:
        with urllib.request.urlopen(request, timeout=45) as response:
            raw = response.read()
            return json.loads(raw) if raw else None
    except urllib.error.HTTPError as error:
        # API bodies can contain account details; the endpoint and status identify this failure.
        raise RuntimeError(f'Nexus {method} {url} failed: HTTP {error.code}; inspect the destination before retrying a write') from None
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as error:
        raise RuntimeError(f'Nexus {method} {url} failed ({type(error).__name__}); inspect the destination before retrying a write') from None


def api_key():
    key = os.environ.get('NEXUSMODS_API_KEY', '').strip()
    if not key:
        raise ValueError('NEXUSMODS_API_KEY is missing; configure the repository or organization secret')
    return key


def api_get(endpoint):
    return request_json(API + endpoint, headers={'apikey': api_key()})['data']


def check_destination(target, version, get=api_get):
    mod = get(f"/games/{target['domain']}/mods/{target['page_id']}")
    if str(mod['id']) != target['mod_uid'] or str(mod['game_id']) != str(target['game_id']) or str(mod['game_scoped_id']) != str(target['page_id']):
        raise ValueError('Nexus page resolved to a different mod/game')
    files = get(f"/mods/{target['mod_uid']}/files")['mod_files']
    if not any(str(row['id']) == target['file_id'] for row in files):
        raise ValueError('Nexus file ID does not belong to the configured mod')
    versions = get(f"/mod-files/{target['file_id']}/versions")['versions']
    if not versions:
        raise ValueError('Nexus file has no existing version; check its file ID')
    for row in versions:
        if str(row['file']['id']) != target['file_id']:
            raise ValueError('Nexus returned a version from another file')
    if any(row['version'] == version for row in versions):
        return 'already-present'
    candidate = version_key(version)
    for row in versions:
        if version_key(row['version']) >= candidate:
            raise ValueError('Release version is older than a version already on Nexus: ' + row['version'])
    return 'ready'


def outputs(values):
    if destination := os.environ.get('GITHUB_OUTPUT'):
        with open(destination, 'a', encoding='utf-8') as stream:
            for name, value in values.items():
                delimiter = 'output_' + uuid.uuid4().hex
                stream.write(f'{name}<<{delimiter}\n{value}\n{delimiter}\n')


def local_release(tag, directory):
    """Check the one product archive in directory against SHA256SUMS.txt, its embedded version and the tag."""
    product, tag_version = product_for(tag)
    target = targets()[product]
    archives = [path for path in directory.iterdir() if fnmatch.fnmatchcase(path.name, target['asset_pattern'])]
    if len(archives) != 1:
        raise ValueError(f'{product}: expected one release archive in {directory}, found {len(archives)}')
    path = archives[0]
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != checksum_for((directory / 'SHA256SUMS.txt').read_text(encoding='utf-8'), path.name):
        raise ValueError('Release archive differs from SHA256SUMS.txt: ' + path.name)
    version = archive_version(path, product)
    if version != tag_version:
        raise ValueError(f'Tag {tag} differs from the archive version {version}')
    expected_name = f'MT2Framework-{version}-windows-x64.zip' if product == 'framework' else f'MMORPGTycoon2-Vortex-{version}.zip'
    if path.name != expected_name:
        raise ValueError('Archive filename differs from embedded product version')
    return {'product': product, 'tag': tag, 'version': version, 'filename': str(path), 'sha256': actual,
            'bytes': path.stat().st_size}


def prepare(tag, directory, get=api_get):
    plan = local_release(tag, directory)
    target = targets()[plan['product']]
    notes = section((ROOT / 'CHANGELOG.md').read_text(encoding='utf-8'), plan['product'], plan['version'])
    plan.update(status=check_destination(target, plan['version'], get), file_id=target['file_id'], mod_uid=target['mod_uid'],
                nexus_url=f"https://www.nexusmods.com/{target['domain']}/mods/{target['page_id']}",
                notes=notes, notes_plain=plain(notes), notes_bbcode=bbcode(notes))
    (directory / 'plan.json').write_text(json.dumps(plan, indent=2) + '\n', encoding='utf-8')
    (directory / 'notes.md').write_text(notes + '\n', encoding='utf-8')
    outputs({'status': plan['status'], 'title': f"{target['name']} {plan['version']}",
             'display_name': target['name'], 'description': plan['notes_bbcode'],
             **{name: plan[name] for name in ('filename', 'version', 'file_id', 'mod_uid')}})
    print(f"info: {plan['product']} {plan['version']} validated, Nexus status {plan['status']}, sha256 {plan['sha256']}")
    return plan


def verify(directory, version_id):
    plan = json.loads((directory / 'plan.json').read_text(encoding='utf-8'))
    row = api_get('/mod-file-versions/' + version_id)
    if str(row['id']) != version_id or str(row['file']['id']) != plan['file_id'] or row['version'] != plan['version'] or row['category'] != 'main':
        raise ValueError('Nexus upload readback differs from the release plan')
    print(f"info: Nexus file version {version_id} confirmed for {plan['product']} {plan['version']}")


def normalized_notes(value, html_encoded=False):
    lines = [line for item in value for line in item.splitlines()] if isinstance(value, list) else value.splitlines()
    if html_encoded:
        lines = [html.unescape(line) for line in lines]
    return ' '.join(re.sub(r'^\s*[-*]\s+', '', line).strip() for line in lines if line.strip())


def changelog_status(existing, version, expected):
    rows = existing.get(version, [])
    if not rows:
        return 'missing'
    if normalized_notes(rows, html_encoded=True) == normalized_notes(expected):
        return 'confirmed'
    raise ValueError(f'Nexus changelog {version} already contains different text; additive API must not append a duplicate')


def sync_changelog(plan, request=request_json):
    """Write the version changelog once: read through v1, write through v3, read back."""
    target = targets()[plan['product']]
    url = f"https://api.nexusmods.com/v1/games/{target['domain']}/mods/{target['page_id']}/changelogs.json"
    headers = {'apikey': api_key()}
    status = changelog_status(request(url, headers=headers), plan['version'], plan['notes_plain'])
    if status == 'missing':
        request(API + f"/mods/{target['mod_uid']}/changelogs", 'POST',
                {'version': plan['version'], 'changelog': plan['notes_plain']}, headers)
        if changelog_status(request(url, headers=headers), plan['version'], plan['notes_plain']) != 'confirmed':
            raise RuntimeError('Nexus accepted the changelog write, but readback is missing; inspect the destination before retrying')
        print(f"info: Nexus changelog {plan['product']} {plan['version']} written and read back")
    else:
        print(f"info: Nexus changelog {plan['product']} {plan['version']} already matches; nothing written")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('prepare', help='Validate the release directory and the Nexus destination')
    command.add_argument('--tag', required=True)
    command.add_argument('--directory', type=Path, required=True)
    command = commands.add_parser('verify', help='Read back the uploaded Nexus file version')
    command.add_argument('--directory', type=Path, required=True)
    command.add_argument('--version-id', required=True)
    command = commands.add_parser('changelog', help='Complete the Nexus changelog of the prepared version')
    command.add_argument('--directory', type=Path, required=True)
    args = parser.parse_args()
    if args.command == 'prepare':
        prepare(args.tag, args.directory)
    elif args.command == 'verify':
        verify(args.directory, args.version_id)
    else:
        sync_changelog(json.loads((args.directory / 'plan.json').read_text(encoding='utf-8')))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, RuntimeError, zipfile.BadZipFile) as error:
        print('error: ' + str(error), file=sys.stderr)
        sys.exit(1)
