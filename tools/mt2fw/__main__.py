"""Optional command-line interface; no arguments opens the desktop helper."""
from pathlib import Path
import argparse
import json
import sys
from . import packages, install


def build_parser():
    parser = argparse.ArgumentParser(prog='mt2fw')
    commands = parser.add_subparsers(dest='command')
    create = commands.add_parser('new')
    create.add_argument('id')
    create.add_argument('directory', type=Path)
    create.add_argument('--template', choices=['hello', 'inspector', 'scenery', 'native'], default='hello')
    check = commands.add_parser('check')
    check.add_argument('directory', type=Path)
    pack = commands.add_parser('pack')
    pack.add_argument('directory', type=Path)
    pack.add_argument('output', type=Path)
    diagnose = commands.add_parser('doctor')
    diagnose.add_argument('--game', type=Path)
    diagnose.add_argument('--profile', type=Path)
    setup = commands.add_parser('install')
    setup.add_argument('package', type=Path)
    setup.add_argument('game', type=Path)
    setup.add_argument('--profile', type=Path)
    remove = commands.add_parser('uninstall')
    remove.add_argument('game', type=Path)
    add = commands.add_parser('add')
    add.add_argument('archive', type=Path)
    add.add_argument('profile', type=Path)
    return parser


def main():
    for stream in (sys.stdout,sys.stderr):
        if stream is not None and hasattr(stream,'reconfigure'):stream.reconfigure(encoding='utf-8')
    args = build_parser().parse_args()
    try:
        if args.command is None:
            from .gui import main as gui
            gui()
            return
        if args.command == 'new':
            result = packages.new_mod(args.directory, args.id, args.template)
        elif args.command == 'check':
            result = packages.check(args.directory)
        elif args.command == 'pack':
            result = packages.pack(args.directory, args.output)
        elif args.command == 'doctor':
            result = install.doctor(args.game, args.profile)
        elif args.command == 'install':
            result = install.install(args.package, args.game, args.profile)
        elif args.command == 'uninstall':
            result = install.uninstall(args.game)
        elif args.command == 'add':
            result = packages.install_mod(args.archive, args.profile)
        print(json.dumps(result, ensure_ascii=False, indent=2))
    except (ValueError, OSError, KeyError) as error:
        print('error: ' + str(error), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
