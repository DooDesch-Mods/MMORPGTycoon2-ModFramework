"""Build the standalone framework and retain exact command output."""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--core-only', action='store_true')
parser.add_argument('--configure-only', action='store_true')
parser.add_argument('--skip-tests', action='store_true', help='Keep a passing unchanged core test scope closed')
args = parser.parse_args()
build = ROOT / 'build'
logs = ROOT / 'artifacts' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
logs.mkdir(parents=True)
commands = [['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Visual Studio 17 2022', '-A', 'x64',
             '-DMT2FW_GAME=' + ('OFF' if args.core_only else 'ON')]]
if not args.configure_only:
    commands += [['cmake', '--build', str(build), '--config', 'Release', '--parallel', '2']]
    if not args.skip_tests:
        commands += [['ctest', '--test-dir', str(build), '-C', 'Release', '--output-on-failure']]
for index, command in enumerate(commands):
    path = logs / f'{index}.log'
    with path.open('w', encoding='utf-8') as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    print(f'{"error" if result.returncode else "info"}: step={index} exit={result.returncode} log={path}', flush=True)
    if result.returncode:
        print('\n'.join(path.read_text(encoding='utf-8', errors='replace').splitlines()[-40:]))
        sys.exit(result.returncode)
