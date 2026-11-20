"""Build the standalone framework and retain exact command output.

--check adds the offline repository checks, --deploy copies the loader and runtime into the game,
--smoke deploys, starts the game through Steam, waits for the framework load line and closes the game.
--live deploys into an owned copy of the game folder, starts it, creates a new world, runs a few feature
requests through the diagnostics channel and exits the game.
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
parser.add_argument('--check', action='store_true', help='Also run the Python, contract and roadmap checks')
parser.add_argument('--deploy', action='store_true', help='Copy WSOCK32.dll and MT2Framework.dll into the game folder')
parser.add_argument('--smoke', action='store_true', help='Deploy, start the game, wait for the framework load line, close the game')
parser.add_argument('--game', type=Path, default=Path(os.environ.get('MT2_GAME_DIR') or STEAM_GAME),
                    help='Game folder with MT2.exe (default: MT2_GAME_DIR or the Steam default path)')
parser.add_argument('--live', action='store_true',
                    help='Deploy into an owned game copy, create a world, run feature requests, exit the game')
args = parser.parse_args()
args.deploy |= args.smoke or args.live
if (args.deploy or args.check) and (args.core_only or args.configure_only):
    parser.error('--check, --deploy, --smoke and --live need the full build')
# The live run creates worlds and removes objects in the profile; a Steam install holds a player's profile.
if args.live and 'steamapps' in (part.lower() for part in args.game.resolve().parts):
    parser.error(f'--live needs an owned copy of the game folder, not the Steam install {args.game}')


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


def live(game):
    """Run feature requests in an owned game copy through `<profile>/diagnostics/request.json`."""
    profile = Path(json.loads((game / 'mt2fw.json').read_text(encoding='utf-8'))['profile'])
    diag = profile / 'diagnostics'
    diag.mkdir(parents=True, exist_ok=True)
    world = 'MPTestLive' + datetime.now().strftime('%m%d%H%M%S')
    env = dict(os.environ, SteamAppId=STEAM_APP, SteamGameId=STEAM_APP)
    proc = subprocess.Popen([str(game / 'MT2.exe')], cwd=game, env=env)
    print(f'info: live started {game / "MT2.exe"} pid={proc.pid} profile={profile} world={world}', flush=True)

    def wait(what, seconds, probe):
        end = time.time() + seconds
        while True:
            value = probe()
            if value:
                return value
            if proc.poll() is not None:
                fail(f'live: game exited with code {proc.returncode} while waiting for {what}')
            if time.time() > end:
                fail(f'live: no {what} within {seconds} s')
            time.sleep(0.2)  # polls the profile files the game writes; returns as soon as they hold the answer

    def read(path):
        try:
            return json.loads(path.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            return None  # absent or mid-write; the caller polls again

    status = wait('status.json from this process', 120, lambda: (s := read(profile / 'status.json')) and s.get('pid') == proc.pid and s)
    print(f'info: live framework {status.get("version")} ready, error={status.get("error")!r}', flush=True)

    def request(op, arguments=None, timeout=30):
        rid = os.urandom(8).hex()
        body = {'id': rid, 'pid': proc.pid, 'process_start_time': status['process_start_time'],
                'operation': op, 'arguments': arguments or {}}
        (diag / 'request.tmp').write_text(json.dumps(body), encoding='utf-8')
        os.replace(diag / 'request.tmp', diag / 'request.json')
        return wait(f'response to {op}', timeout, lambda: (r := read(diag / 'response.json')) and r.get('id') == rid and r)

    def step(label, op, arguments, passed, seconds=0):
        """Send op until passed(response) or seconds run out; one try when seconds is 0."""
        end = time.time() + seconds
        while True:
            response = request(op, arguments)
            if passed(response):
                print(f'info: live {label} ok: {json.dumps(response.get("result"))[:300]}', flush=True)
                return response
            if time.time() >= end:
                fail(f'live {label} failed: {json.dumps(response)[:500]}')
            time.sleep(1)  # the world is still loading; asks again

    try:
        step('prepare_world', 'prepare_world', {'name': world}, lambda r: r['ok'], 120)
        step('map layout', 'world_ready', {}, lambda r: r['ok'] and r['result']['ready'], 180)
        step('create_world', 'create_world', {'name': world}, lambda r: r['ok'])
        step('world loaded', 'world.status', {}, lambda r: r['ok'] and r['result']['loaded'], 240)
        for op in ('network.catalogue', 'network.snapshot'):
            step(op, 'gameplay.query', {'operation': op, 'arguments': {}}, lambda r: r['ok'])
        rows = step('objects.list', 'objects.list', {'limit': 16}, lambda r: r['ok'] and r['result'])['result']
        step('native remove', 'native_remove_once', {'handle': rows[0]['handle']},
             lambda r: r['ok'] and r['result']['status'] == 'confirmed')
        rid = step('capture', 'capture', {}, lambda r: r['ok'])['id']
        receipt = wait('capture.json', 30, lambda: (c := read(diag / 'capture.json')) and c.get('id') == rid and c)
        print(f'info: live capture {receipt["width"]}x{receipt["height"]} {receipt["path"]} sha256={receipt["sha256"]}', flush=True)
    finally:
        if proc.poll() is None:
            try:
                # The game accepts exit only on the idle title screen; at the title already, this request fails harmlessly.
                response = request('title', timeout=10)
                print(f'info: live return to title ok={response["ok"]} {response.get("error", "")}', flush=True)
                step('exit', 'exit', {}, lambda r: r['ok'], 120)
                proc.wait(30)
            except (SystemExit, subprocess.TimeoutExpired):
                print(f'warn: live game did not exit on request; killing pid {proc.pid}', flush=True)
                proc.kill()
                proc.wait()
        print(f'info: live game exited with code {proc.returncode}', flush=True)


build = ROOT / 'build'
logs = ROOT / 'artifacts' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
logs.mkdir(parents=True)
cmake = find_cmake()
print(f'info: cmake={cmake}', flush=True)
# CMake reads CMAKE_GENERATOR, CMAKE_<LANG>_COMPILER_LAUNCHER and CMAKE_BUILD_PARALLEL_LEVEL from the
# environment; CI sets them for Ninja Multi-Config with sccache. Without them the Visual Studio build stays.
generator = [] if os.environ.get('CMAKE_GENERATOR') else ['-G', 'Visual Studio 17 2022', '-A', 'x64']
parallel = [] if os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL') else ['--parallel', '2']
print(f'info: generator={os.environ.get("CMAKE_GENERATOR") or "Visual Studio 17 2022"} '
      f'jobs={os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or 2} '
      f'launcher={os.environ.get("CMAKE_CXX_COMPILER_LAUNCHER") or "none"}', flush=True)
commands = [[str(cmake), '-S', str(ROOT), '-B', str(build), *generator,
             '-DMT2FW_GAME=' + ('OFF' if args.core_only else 'ON')]]
if not args.configure_only:
    commands += [[str(cmake), '--build', str(build), '--config', 'Release', *parallel]]
    if not args.skip_tests:
        commands += [[str(cmake.with_name('ctest.exe')), '--test-dir', str(build), '-C', 'Release', '--output-on-failure']]
if args.check:
    commands += [[sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'test_*.py'],
                 [sys.executable, 'tools/generate_docs.py', '--check'],
                 [sys.executable, 'tools/check_contract.py'],
                 [sys.executable, 'tools/render_roadmap.py', '--check']]
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
    if args.live:
        live(args.game.resolve())
