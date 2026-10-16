"""Build a portable helper and CLI in a workspace-local Python environment."""
from pathlib import Path
from datetime import datetime, timezone
import argparse
import shutil
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bootstrap', action='store_true', help='Install pinned build dependencies into the local venv')
args = parser.parse_args()
environment = ROOT / 'build/helper-env'
python = environment / 'Scripts/python.exe'
logs = ROOT / 'artifacts/helper-build' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
logs.mkdir(parents=True)
if not python.is_file():
    if not args.bootstrap:
        raise SystemExit('error: helper environment is missing; use --bootstrap')
    venv.EnvBuilder(with_pip=True).create(environment)
commands = []
if args.bootstrap:
    commands.append([str(python), '-m', 'pip', 'install', '--disable-pip-version-check', 'pyinstaller==6.22.3', 'Pillow==11.3.0'])
for name, mode in (('mt2fw', '--console'), ('MT2FW-Helper', '--windowed')):
    command = [str(python), '-m', 'PyInstaller', '--noconfirm', '--onefile', mode, '--name', name,
               '--distpath', str(ROOT / 'dist/helper'), '--workpath', str(ROOT / 'build/helper-work' / name),
               '--specpath', str(ROOT / 'build/helper-spec'), '--paths', str(ROOT / 'tools')]
    docs=ROOT/'dist/docs/offline'
    if not (docs/'quickstart.html').is_file():raise SystemExit('error: build offline documentation first with npm run docs:build')
    for source, destination in ((ROOT / 'templates', 'templates'), (ROOT / 'sdk', 'sdk'),
                                (ROOT / 'include/mt2fw/plugin.h', 'include/mt2fw'), (docs, 'docs'),
                                (ROOT/'api/contract.json','api'),(ROOT/'LICENSE','.')):
        command += ['--add-data', str(source) + ';' + destination]
    command += [str(ROOT / 'tools/launcher.py')]
    commands.append(command)
for index, command in enumerate(commands):
    path = logs / f'{index}.log'
    with path.open('w', encoding='utf-8') as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print(f'{"error" if result.returncode else "info"}: helper step={index} exit={result.returncode} log={path}', flush=True)
    if result.returncode:
        print('\n'.join(path.read_text(encoding='utf-8', errors='replace').splitlines()[-25:]))
        sys.exit(result.returncode)
print('info: portable helper and CLI built in ' + str(ROOT / 'dist/helper'))
