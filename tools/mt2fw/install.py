"""Install only package-owned framework files; retain profiles and user saves."""
from __future__ import annotations
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
from datetime import datetime, timezone
from .packages import PackageError, atomic_json, read_json, confined

EXE_SHA256 = 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6'
RECEIPT = 'mt2fw-install.json'


def require_game_closed(game: Path):
    if os.name!='nt':return
    import ctypes
    from ctypes import wintypes
    class Process(ctypes.Structure):
        _fields_=[('size',wintypes.DWORD),('usage',wintypes.DWORD),('pid',wintypes.DWORD),('heap',ctypes.c_size_t),('module',wintypes.DWORD),('threads',wintypes.DWORD),('parent',wintypes.DWORD),('priority',wintypes.LONG),('flags',wintypes.DWORD),('name',wintypes.WCHAR*260)]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes=[wintypes.DWORD,wintypes.DWORD];kernel.CreateToolhelp32Snapshot.restype=wintypes.HANDLE
    kernel.Process32FirstW.argtypes=[wintypes.HANDLE,ctypes.POINTER(Process)];kernel.Process32NextW.argtypes=kernel.Process32FirstW.argtypes
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    snapshot=kernel.CreateToolhelp32Snapshot(2,0)
    if snapshot==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
    try:
        row=Process();row.size=ctypes.sizeof(row);present=kernel.Process32FirstW(snapshot,ctypes.byref(row))
        while present:
            if row.name.casefold()=='mt2.exe':
                process=kernel.OpenProcess(0x1000,False,row.pid)
                if not process:raise PackageError('MT2 is running and its path cannot be checked; close the game before changing framework files')
                try:
                    buffer=ctypes.create_unicode_buffer(32768);length=wintypes.DWORD(len(buffer))
                    if not kernel.QueryFullProcessImageNameW(process,0,buffer,ctypes.byref(length)):raise ctypes.WinError(ctypes.get_last_error())
                    if Path(buffer.value).resolve()==(game/'MT2.exe').resolve():raise PackageError('Close this MT2 game before installing, updating or removing the framework')
                finally:
                    if not kernel.CloseHandle(process):raise ctypes.WinError(ctypes.get_last_error())
            present=kernel.Process32NextW(snapshot,ctypes.byref(row))
        if ctypes.get_last_error()!=18:raise ctypes.WinError(ctypes.get_last_error())
    finally:
        if not kernel.CloseHandle(snapshot):raise ctypes.WinError(ctypes.get_last_error())


def framework_name(name):
    if name not in ('WSOCK32.dll', 'MT2Framework.dll') and not name.startswith('mt2fw/'):
        raise PackageError('File is outside the framework installation allowlist: ' + name)


def sha(path: Path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def steam_games() -> list[Path]:
    roots = []
    if os.name == 'nt':
        import winreg
        for hive, key in ((winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam'),
                          (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\Valve\Steam')):
            try:
                with winreg.OpenKey(hive, key) as handle:
                    for name in ('SteamPath', 'InstallPath'):
                        try:
                            roots.append(Path(winreg.QueryValueEx(handle, name)[0]))
                        except FileNotFoundError:
                            continue
            except FileNotFoundError:
                continue
    libraries = set(roots)
    for root in roots:
        path = root / 'steamapps' / 'libraryfolders.vdf'
        if path.is_file():
            for value in re.findall(r'"path"\s+"([^"]+)"', path.read_text(encoding='utf-8')):
                libraries.add(Path(value.replace('\\\\', '\\')))
    return sorted({root / 'steamapps/common/MMORPG Tycoon 2' for root in libraries
                   if (root / 'steamapps/common/MMORPG Tycoon 2/MT2.exe').is_file()})


def default_profile(game: Path) -> Path:
    local = os.environ.get('LOCALAPPDATA')
    if not local:
        raise PackageError('LOCALAPPDATA is unavailable; select a profile explicitly')
    key = hashlib.sha256(str(game.resolve()).casefold().encode('utf-8')).hexdigest()[:16]
    return Path(local) / 'MT2Framework' / key / 'profiles/default'


def classic_mods(game: Path | None,profile: Path | None) -> list[dict]:
    user=Path(os.environ.get('APPDATA',''))/'VectorStorm/MMORPG Tycoon 2'
    roots=[user]
    if profile is not None:roots.append(profile/'game-data')
    candidates=[]
    for root in roots:
        directories=[root/'mod']
        if root.is_dir():directories.extend(p/'mod' for p in root.iterdir() if p.is_dir() and p.name.isdigit())
        for directory in directories:
            if directory.is_dir():
                candidates.extend({'name':p.name,'path':str(p),'kind':'legacy','managed':False} for p in directory.iterdir() if p.is_dir())
    if game is not None and (game/'Data').is_dir():
        candidates.extend({'name':p.name,'path':str(p),'kind':'loose assets','managed':False} for p in (game/'Data').iterdir() if p.is_dir())
    return sorted({row['path'].casefold():row for row in candidates}.values(),key=lambda row:row['path'].casefold())


def doctor(game: Path | None = None, profile: Path | None = None) -> dict:
    result = {'version': '0.1.2', 'steam_installations': [str(p) for p in steam_games()], 'issues': []}
    if game is not None:
        if not (game / 'MT2.exe').is_file():
            result['issues'].append('MT2.exe not found')
        else:
            result['executable_sha256'] = sha(game / 'MT2.exe')
            if result['executable_sha256'] != EXE_SHA256:
                result['issues'].append('Unsupported game build; game adapter and mods will stay disabled')
        receipt = game / RECEIPT
        if (game / 'WSOCK32.dll').exists() and not receipt.exists():
            result['issues'].append('Existing loader has no framework receipt; automatic overwrite is refused')
        if receipt.exists():
            for name, expected in read_json(receipt)['files'].items():
                path = confined(game, name)
                if not path.is_file() or sha(path) != expected:
                    result['issues'].append('Installed framework file differs: ' + name)
    if profile is not None:
        result['profile'] = str(profile)
        result['save_imports']=[read_json(p) for p in sorted((profile/'imports').glob('*/job.json'))]
        status = profile / 'status.json'
        if status.exists():
            result['runtime'] = read_json(status)
        result['classic_mods'] = classic_mods(game,profile)
    return result


def install(package: Path, game: Path, profile: Path | None = None) -> dict:
    game = game.resolve()
    require_game_closed(game)
    if not (game / 'MT2.exe').is_file() or sha(game / 'MT2.exe') != EXE_SHA256:
        raise PackageError('Select the supported MT2 Steam installation')
    manifest = read_json(package / 'framework-package.json')
    expected = manifest['files']
    if not {'WSOCK32.dll', 'MT2Framework.dll'}.issubset(expected):
        raise PackageError('Framework package misses loader or runtime')
    previous = read_json(game / RECEIPT) if (game / RECEIPT).is_file() else {'files': {}}
    for name, checksum in expected.items():
        framework_name(name)
        source, target = confined(package, name), confined(game, name)
        if not source.is_file() or sha(source) != checksum:
            raise PackageError('Package file SHA256 differs: ' + name)
        if target.exists() and (name not in previous['files'] or sha(target) != previous['files'][name]):
            raise PackageError('Refusing to overwrite unknown or modified file: ' + name)
    for name, checksum in previous['files'].items():
        framework_name(name)
        target = confined(game, name)
        if not target.is_file() or sha(target) != checksum:
            raise PackageError('Installed framework file differs; preserve and inspect before update: ' + name)
    profile = (profile or default_profile(game)).absolute()
    profile.mkdir(parents=True, exist_ok=True)
    for name in ('mods', 'data', 'saves', 'game-data', 'logs', 'diagnostics'):
        confined(profile, name).mkdir(exist_ok=True)
    backup = profile / 'backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup.mkdir(parents=True)
    transaction_names = sorted(set(previous['files']) | set(expected) | {'mt2fw.json', RECEIPT})
    for name in transaction_names:
        target = confined(game, name)
        if target.is_file():
            copy = confined(backup, name)
            copy.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, copy)
    written = []
    try:
        for name in expected:
            target = confined(game, name)
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_name(target.name + '.mt2fw-tmp')
            shutil.copy2(confined(package, name), temporary)
            os.replace(temporary, target)
            written.append(name)
        atomic_json(game / 'mt2fw.json', {'profile': str(profile), 'enabled': True})
        receipt = {'version': manifest['version'], 'files': expected, 'profile': str(profile), 'backup': str(backup)}
        atomic_json(game / RECEIPT, receipt)
        for name in set(previous['files']) - set(expected):
            confined(game, name).unlink()
        return receipt
    except Exception:
        for name in reversed(transaction_names):
            target = confined(game, name)
            original = confined(backup, name)
            if original.exists():
                shutil.copy2(original, target)
            elif target.exists():
                target.unlink()
        raise


def uninstall(game: Path) -> dict:
    require_game_closed(game)
    receipt = read_json(game / RECEIPT)
    for name, expected in receipt['files'].items():
        framework_name(name)
        path = confined(game, name)
        if path.exists() and sha(path) != expected:
            raise PackageError('Modified framework file preserved; removal refused: ' + name)
    removed = []
    for name in receipt['files']:
        path = confined(game, name)
        if path.exists():
            path.unlink()
            removed.append(name)
    (game / RECEIPT).unlink()
    # The profile and configuration remain available for recovery and reinstallation.
    return {'removed': removed, 'profile_preserved': receipt['profile']}


def import_save(source: Path, profile: Path, world: str):
    import uuid
    if source.suffix.lower() != '.vrz' or source.name.endswith('.meta.vrz') or '/' in world or '\\' in world:
        raise PackageError('Select a world .vrz save and one world name')
    confined(profile, 'imports/' + world)
    account_root = profile / 'game-data'
    accounts = [p for p in account_root.iterdir() if p.is_dir() and p.name.isdigit()] if account_root.exists() else []
    if len(accounts)>1:
        raise PackageError('Profile contains several Steam accounts; select a separate profile for this account')
    identifier = str(uuid.uuid4())
    stage = confined(profile, 'imports/' + identifier)
    stage.mkdir(parents=True)
    files = {}
    sources = [source]
    companion = source.with_name(source.stem + '.meta.vrz')
    if companion.exists(): sources.append(companion)
    rules = source.parent / 'rules.vrt'
    if rules.exists(): sources.append(rules)
    for item in sources:
        destination = stage / item.name
        shutil.copy2(item, destination)
        files[item.name] = sha(destination)
        if files[item.name] != sha(item):
            raise PackageError('Copied save SHA256 differs; original is unchanged')
    job = {'id': identifier, 'world': world, 'files': files, 'state': 'pending'}
    if accounts:
        for name in files:
            if confined(accounts[0], 'save/' + world + '/' + name).exists():
                raise PackageError('World destination already exists; select another world name')
        for name in files:
            destination = confined(accounts[0], 'save/' + world + '/' + name)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(stage / name, destination)
            if sha(destination) != files[name]: raise PackageError('Imported save SHA256 differs')
        job['state'] = 'complete'
        job['account'] = accounts[0].name
    atomic_json(stage / 'job.json', job)
    return {'source': str(source), 'copy': str(stage), 'sha256': files[source.name], 'state': job['state']}
