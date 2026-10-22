"""Shared, non-executing validation for the CLI and desktop helper."""
from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import sys
import tempfile
import zipfile

ID = re.compile(r'[a-z][a-z0-9]*(?:\.[a-z0-9][a-z0-9_-]*)+')
VERSION = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?')
CAPABILITIES = {'ui', 'input', 'world.read', 'objects.read', 'objects.write', 'camera', 'time.write', 'content'}
RESERVED = {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)), *(f'LPT{i}' for i in range(1, 10))}
FRAMEWORK = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[2]))
MAX_FILES = 4096
MAX_BYTES = 512 * 1024 * 1024


class PackageError(ValueError):
    pass


def version(text: str) -> tuple:
    found = VERSION.fullmatch(text) if isinstance(text, str) else None
    if not found:
        raise PackageError(f'Expected SemVer, got: {text!r}')
    parts = found[4].split('.') if found[4] else []
    if any(p.isdigit() and len(p)>1 and p.startswith('0') for p in parts):
        raise PackageError('SemVer numeric prerelease identifier has a leading zero')
    suffix = tuple((0, int(p)) if p.isdigit() else (1, p) for p in parts)
    return (*map(int, found.groups()[:3]), 0 if parts else 1, suffix)


def matches(actual: str, required: str) -> bool:
    current = version(actual)
    if required.startswith('^'):
        minimum = version(required[1:])
        last = 0 if minimum[0] else 1 if minimum[1] else 2
        return minimum <= current and current[:last+1] == minimum[:last+1]
    if required.startswith('>='):
        return current >= version(required[2:])
    return current == version(required)


def relative(name: str) -> Path:
    if not isinstance(name, str) or not name or '\\' in name or '\x00' in name or ':' in name:
        raise PackageError(f'Invalid relative path: {name!r}')
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in ('', '.', '..') for part in name.split('/')):
        raise PackageError(f'Path escapes package: {name!r}')
    for part in path.parts:
        if part.endswith(('.', ' ')) or part.split('.')[0].upper() in RESERVED or any(c in part for c in '<>"|?*'):
            raise PackageError(f'Invalid Windows path: {name!r}')
    return Path(*path.parts)


def confined(root: Path, name: str) -> Path:
    root = root.absolute()
    path = root / relative(name)
    for part in (root, *path.relative_to(root).parents, path.relative_to(root)):
        candidate = part if part == root else root / part
        if candidate.is_symlink() or (hasattr(candidate, 'is_junction') and candidate.is_junction()):
            raise PackageError(f'Linked package path refused: {candidate}')
    if not path.resolve().is_relative_to(root.resolve()):
        raise PackageError(f'Path escapes root: {path}')
    return path


def read_json(path: Path):
    if path.stat().st_size > 4 * 1024 * 1024:
        raise PackageError(f'JSON exceeds 4 MiB: {path}')
    if os.name=='nt':
        import ctypes
        import msvcrt
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
        kernel.CreateFileW.restype=ctypes.c_void_p
        handle=kernel.CreateFileW(str(path.absolute()),0x80000000,7,None,3,0x80,None)
        if handle==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
        try:descriptor=msvcrt.open_osfhandle(handle,os.O_RDONLY|os.O_BINARY)
        except Exception:
            kernel.CloseHandle.argtypes=[ctypes.c_void_p];kernel.CloseHandle(handle);raise
        with os.fdopen(descriptor,'r',encoding='utf-8') as stream:return json.load(stream)
    return json.loads(path.read_text(encoding='utf-8'))


def atomic_json(path: Path, value):
    encoded = json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + '\n'
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(encoded, encoding='utf-8')
    if os.name=='nt' and path.exists():
        import ctypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.ReplaceFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_void_p]
        if not kernel.ReplaceFileW(str(path.absolute()),str(temp.absolute()),None,0,None,None):raise ctypes.WinError(ctypes.get_last_error())
    else:os.replace(temp,path)


def validate_manifest(data: dict) -> dict:
    if not isinstance(data, dict):
        raise PackageError('mod.json must contain an object')
    for key in ('id', 'name', 'version', 'api_version', 'type', 'entry'):
        value = data.get(key)
        if not isinstance(value, str) or not value or len(value.encode('utf-8')) > 256 or '\x00' in value:
            raise PackageError(f'Missing or invalid string: {key}')
    if not ID.fullmatch(data['id']):
        raise PackageError('Mod ID must be lowercase and namespaced, for example author.my_mod')
    version(data['version'])
    api = version(data['api_version'])
    if api[0] != 1 or api > version('1.2.0'):
        raise PackageError('Mod requires an unsupported API version')
    if data['type'] not in ('lua', 'native', 'content'):
        raise PackageError('Mod type must be lua, native or content')
    relative(data['entry'])
    mode = data.get('multiplayer', 'singleplayer')
    if mode not in ('local', 'shared', 'singleplayer'):
        raise PackageError('Invalid multiplayer class')
    caps = data.get('capabilities')
    if not isinstance(caps, list) or any(not isinstance(c, str) or c not in CAPABILITIES for c in caps):
        raise PackageError('capabilities must list known capability names')
    if mode == 'local' and set(caps) & {'objects.write', 'time.write', 'content'}:
        raise PackageError('Local mods cannot change the shared world')
    dependencies = data.get('dependencies', {})
    if not isinstance(dependencies, dict):
        raise PackageError('dependencies must map IDs to versions')
    for dependency, constraint in dependencies.items():
        if not ID.fullmatch(dependency) or not isinstance(constraint, str):
            raise PackageError(f'Invalid dependency: {dependency}')
        matches('1.0.0', constraint)
    return data


def files(root: Path):
    total = 0
    names = set()
    count = 0
    for path in sorted(root.rglob('*')):
        name = path.relative_to(root).as_posix()
        if any(part in ('.git', '__pycache__', 'build') for part in path.relative_to(root).parts):
            continue
        confined(root, name)
        if not path.is_file():
            continue
        key = name.casefold()
        if key in names:
            raise PackageError(f'Case-insensitive path conflict: {name}')
        names.add(key)
        count += 1
        total += path.stat().st_size
        if count > MAX_FILES or total > MAX_BYTES:
            raise PackageError('Package exceeds 4096 files or 512 MiB')
        yield path, name


def check(root: Path) -> dict:
    root = root.absolute()
    manifest = validate_manifest(read_json(confined(root, 'mod.json')))
    entry = confined(root, manifest['entry'])
    if not entry.is_file():
        raise PackageError(f'Entry file missing: {manifest["entry"]}')
    package_files = list(files(root))
    if manifest['type'] == 'lua' and entry.suffix != '.lua':
        raise PackageError('Lua entry must end in .lua')
    if manifest['type'] == 'native' and entry.suffix.lower() != '.dll':
        raise PackageError('Native entry must end in .dll')
    if manifest['type'] == 'content':
        definitions = read_json(entry)
        if not isinstance(definitions, list) or not definitions:
            raise PackageError('Content entry must contain a nonempty list')
        seen = set()
        for definition in definitions:
            identifier = definition.get('id', '')
            if not identifier.startswith(manifest['id'] + ':') or identifier in seen:
                raise PackageError(f'Content ID must be unique and namespaced: {identifier}')
            seen.add(identifier)
            for key in ('model', 'material'):
                if not confined(root, definition[key]).is_file():
                    raise PackageError(f'Content {key} missing: {definition[key]}')
    return {'manifest': manifest, 'files': len(package_files), 'bytes': sum(p.stat().st_size for p, _ in package_files)}


def pack(root: Path, output: Path) -> dict:
    result = check(root)
    if output.resolve().is_relative_to(root.resolve()):
        raise PackageError('Write the ZIP outside the mod directory')
    if output.exists():
        raise PackageError(f'Output already exists: {output}')
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'x', zipfile.ZIP_DEFLATED) as archive:
        for source, name in files(root):
            archive.write(source, name)
    result['sha256'] = hashlib.sha256(output.read_bytes()).hexdigest()
    result['archive'] = str(output)
    return result


def install_mod(archive: Path, profile: Path) -> dict:
    mods = profile / 'mods'
    mods.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='package-', dir=profile) as temporary:
        stage = Path(temporary)
        with zipfile.ZipFile(archive) as package:
            total, names = 0, set()
            for info in package.infolist():
                if info.is_dir():
                    relative(info.filename.rstrip('/'))
                    continue
                destination = confined(stage, info.filename)
                key = info.filename.casefold()
                if key in names or stat.S_ISLNK(info.external_attr >> 16):
                    raise PackageError(f'Duplicate or linked ZIP path: {info.filename}')
                names.add(key)
                total += info.file_size
                if len(names) > MAX_FILES or total > MAX_BYTES:
                    raise PackageError('ZIP exceeds 4096 files or 512 MiB')
                destination.parent.mkdir(parents=True, exist_ok=True)
                with package.open(info) as source, destination.open('xb') as output:
                    shutil.copyfileobj(source, output)
        result = check(stage)
        destination = confined(mods, result['manifest']['id'])
        backup = None
        if destination.exists():
            previous = check(destination)
            if previous['manifest']['id'] != result['manifest']['id']:
                raise PackageError('Existing mod directory belongs to another package')
            backup = profile / 'backups' / (destination.name + '-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
            backup.parent.mkdir(exist_ok=True)
            destination.rename(backup)
        try:
            shutil.copytree(stage, destination)
        except Exception:
            if destination.exists():
                shutil.rmtree(destination)
            if backup is not None:
                backup.rename(destination)
            raise
        result.update(directory=str(destination), restart_required=True, backup=str(backup) if backup else None)
        return result


def new_mod(destination: Path, identifier: str, template: str = 'hello') -> dict:
    if not ID.fullmatch(identifier):
        raise PackageError('Use a lowercase namespaced ID, for example author.my_mod')
    if destination.exists():
        raise PackageError(f'Destination already exists: {destination}')
    if template not in ('hello', 'inspector', 'scenery', 'native'):
        raise PackageError(f'Unknown template: {template}')
    source = FRAMEWORK / 'templates' / template
    if template == 'scenery':
        from .glb import GLB, export
        export(GLB(source / 'source.glb').mesh(), destination, identifier, 'pyramid')
        shutil.copy2(source / 'source.glb', destination / 'source.glb')
    else:
        shutil.copytree(source, destination)
    manifest = read_json(destination / 'mod.json')
    manifest['id'] = identifier
    manifest['name'] = identifier.rsplit('.', 1)[-1].replace('_', ' ').title()
    atomic_json(destination / 'mod.json', manifest)
    if template == 'native':
        plugin = destination / 'plugin.cpp'
        plugin.write_text(plugin.read_text(encoding='utf-8').replace('example.native', identifier), encoding='utf-8')
    if template == 'scenery':
        definitions = read_json(destination / 'content.json')
        for definition in definitions:
            definition['id'] = identifier + ':' + definition['id'].split(':')[-1]
        atomic_json(destination / 'content.json', definitions)
    sdk = destination / '.mt2fw'
    sdk.mkdir()
    shutil.copy2(FRAMEWORK / 'sdk' / 'mt2.lua', sdk / 'mt2.lua')
    shutil.copy2(FRAMEWORK / 'LICENSE', destination / 'LICENSE')
    shutil.copy2(FRAMEWORK / 'templates' / 'AGENTS.md', destination / 'AGENTS.md')
    shutil.copy2(FRAMEWORK / 'api' / 'contract.json', sdk / 'api.json')
    (sdk / 'mt2fw').mkdir()
    shutil.copy2(FRAMEWORK / 'include' / 'mt2fw' / 'plugin.h', sdk / 'mt2fw' / 'plugin.h')
    atomic_json(destination / '.vscode' / 'settings.json', {'Lua.workspace.library': ['.mt2fw'], 'Lua.runtime.version': 'Lua 5.4'})
    return {'directory': str(destination), 'id': identifier, 'template': template}
