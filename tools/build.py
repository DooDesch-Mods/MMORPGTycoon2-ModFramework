"""Build the standalone framework and retain exact command output.

--check adds the offline repository checks, --deploy copies the loader and runtime into the game,
--smoke deploys, starts the game through Steam, waits for the framework load line and closes the game.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
STEAM_GAME = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Steam/steamapps/common/MMORPG Tycoon 2'
STEAM_APP = '486860'
SMOKE_TIMEOUT = 180
# The loader looks this export up by name and the runtime exports it, so both files carry the string.
# A WSOCK32.dll or MT2Framework.dll without it belongs to something else and is never overwritten.
FRAMEWORK_MARKER = b'MT2FW_InitInstalled'
parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--core-only', action='store_true')
parser.add_argument('--configure-only', action='store_true')
parser.add_argument('--skip-tests', action='store_true', help='Skip CTest')
parser.add_argument('--check', action='store_true', help='Also run the Python, contract, roadmap and Vortex checks')
parser.add_argument('--deploy', action='store_true', help='Copy WSOCK32.dll and MT2Framework.dll into the game folder')
parser.add_argument('--smoke', action='store_true', help='Deploy, start the game, wait for the framework load line, close the game')
parser.add_argument('--game', type=Path, default=Path(os.environ.get('MT2_GAME_DIR') or STEAM_GAME),
                    help='Game folder with MT2.exe (default: MT2_GAME_DIR or the Steam default path)')
args = parser.parse_args()
args.deploy |= args.smoke
if (args.deploy or args.check) and (args.core_only or args.configure_only):
    parser.error('--check, --deploy and --smoke need the full build')


def fail(message):
    print('error: ' + message, flush=True)
    sys.exit(1)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def find_cmake():
    found = shutil.which('cmake')
    if found:
        return Path(found)
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if not vswhere.is_file():
        fail(f'cmake is not in PATH and {vswhere} is missing; install CMake or Visual Studio 2022 with C++')
    result = subprocess.run([str(vswhere), '-latest', '-products', '*', '-find',
                             r'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'],
                            capture_output=True, text=True, errors='replace')
    paths = result.stdout.splitlines()
    if result.returncode or not paths:
        fail(f'cmake is not in PATH and vswhere found no Visual Studio CMake (exit {result.returncode}): {result.stderr.strip()}')
    return Path(paths[0])


def game_running():
    result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq MT2.exe', '/NH'], capture_output=True, text=True, errors='replace')
    return 'MT2.exe' in result.stdout


def deploy(game):
    if not (game / 'MT2.exe').is_file():
        fail(f'no MT2.exe in {game}; pass --game or set MT2_GAME_DIR')
    if game_running():
        fail('MT2.exe is running and holds the framework DLLs; close the game first')
    hashes = {}
    for name in ('WSOCK32.dll', 'MT2Framework.dll'):
        source, target = ROOT / 'build/bin/Release' / name, game / name
        if not source.is_file():
            fail(f'build output missing: {source}')
        if target.is_file() and FRAMEWORK_MARKER not in target.read_bytes():
            fail(f'{target} is not a framework file; move it away yourself, it is not overwritten')
        hashes[name] = sha256(source)
        try:
            # Unlink first: a Vortex hardlink would otherwise carry the new bytes into its staging copy.
            target.unlink(missing_ok=True)
            shutil.copy2(source, target)
        except OSError as error:
            fail(f'cannot write {target}: {error}')
        deployed = sha256(target)
        if deployed != hashes[name]:
            fail(f'{target} sha256={deployed} differs from build {source} sha256={hashes[name]}')
        print(f'info: deployed {source} -> {target} sha256={deployed}', flush=True)
    config = game / 'mt2fw.json'
    if config.is_file():
        print(f'info: kept existing {config}: {config.read_text(encoding="utf-8").strip()}', flush=True)
    else:
        config.write_text(json.dumps({'enabled': True, 'profile': str(game / 'mt2fw-profile')}) + '\n', encoding='utf-8')
        print(f'info: wrote {config} with profile {game / "mt2fw-profile"}', flush=True)
    return hashes


def smoke(game, hashes):
    profile = Path(json.loads((game / 'mt2fw.json').read_text(encoding='utf-8'))['profile'])
    version = json.loads((ROOT / 'api/contract.json').read_text(encoding='utf-8'))['framework_version']
    wanted = f'MT2 Mod Framework {version} loaded'
    bootstrap = profile / 'bootstrap.jsonl'
    bootstrap_start = bootstrap.stat().st_size if bootstrap.is_file() else 0
    started = time.time()
    os.startfile(f'steam://rungameid/{STEAM_APP}')
    print(f'info: started MT2 through Steam app {STEAM_APP}; waiting up to {SMOKE_TIMEOUT} s for "{wanted}" '
          f'in {profile / "game-data"}\\<account>\\log.txt', flush=True)
    found = None
    while not found and time.time() - started < SMOKE_TIMEOUT:
        time.sleep(1)  # polls the game log for the real signal; the loop ends as soon as the line exists
        # The game rewrites log.txt on start, so a file written after the launch holds only this run.
        for log in profile.glob('game-data/*/log.txt'):
            if log.stat().st_mtime >= started:
                line = next((line for line in log.read_text(encoding='utf-8', errors='replace').splitlines() if wanted in line), None)
                if line:
                    found = (log, line.strip())
    took = time.time() - started
    stop_game()
    if not found:
        tail = bootstrap.read_bytes()[bootstrap_start:].decode('utf-8', 'replace').strip() if bootstrap.is_file() else ''
        fail(f'no "{wanted}" after {took:.0f} s; bootstrap.jsonl since launch: {tail or "(nothing)"}')
    print(f'info: framework loaded after {took:.0f} s: {found[0]}: {found[1]}', flush=True)
    for name, digest in hashes.items():
        print(f'info: game runs {game / name} sha256={sha256(game / name)}, build sha256={digest}', flush=True)


def stop_game():
    # taskkill without /F sends WM_CLOSE, so the game shuts down like a window close.
    result = subprocess.run(['taskkill', '/IM', 'MT2.exe'], capture_output=True, text=True, errors='replace')
    print(f'info: asked MT2.exe to close (taskkill exit {result.returncode}) {result.stdout.strip()} {result.stderr.strip()}', flush=True)
    for _ in range(30):
        if not game_running():
            print('info: MT2.exe exited', flush=True)
            return
        time.sleep(1)
    result = subprocess.run(['taskkill', '/F', '/IM', 'MT2.exe'], capture_output=True, text=True, errors='replace')
    print(f'warn: MT2.exe ignored the close request for 30 s; forced (taskkill exit {result.returncode})', flush=True)


build = ROOT / 'build'
logs = ROOT / 'artifacts' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
logs.mkdir(parents=True)
cmake = find_cmake()
print(f'info: cmake={cmake}', flush=True)
commands = [[str(cmake), '-S', str(ROOT), '-B', str(build), '-G', 'Visual Studio 17 2022', '-A', 'x64',
             '-DMT2FW_GAME=' + ('OFF' if args.core_only else 'ON')]]
if not args.configure_only:
    commands += [[str(cmake), '--build', str(build), '--config', 'Release', '--parallel', '2']]
    if not args.skip_tests:
        commands += [[str(cmake.with_name('ctest.exe')), '--test-dir', str(build), '-C', 'Release', '--output-on-failure']]
if args.check:
    commands += [[sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'test_*.py'],
                 [sys.executable, 'tools/generate_docs.py', '--check'],
                 [sys.executable, 'tools/check_contract.py'],
                 [sys.executable, 'tools/render_roadmap.py', '--check'],
                 ['node', '--test', 'tests/vortex.cjs']]
for index, command in enumerate(commands):
    path = logs / f'{index}.log'
    started = time.time()
    with path.open('w', encoding='utf-8') as stream:
        try:
            result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
        except OSError as error:
            fail(f'step={index} cannot start {command[0]}: {error}')
    print(f'{"error" if result.returncode else "info"}: step={index} exit={result.returncode} '
          f'seconds={time.time() - started:.1f} log={path} command={" ".join(command[:3])}', flush=True)
    if result.returncode:
        print('\n'.join(path.read_text(encoding='utf-8', errors='replace').splitlines()[-40:]))
        sys.exit(result.returncode)
if args.deploy:
    hashes = deploy(args.game.resolve())
    if args.smoke:
        smoke(args.game.resolve(), hashes)
