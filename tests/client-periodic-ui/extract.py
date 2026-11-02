"""Retain real UI guards and state handling; replace only the native call in the fixture."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
text = args.source.read_text(encoding='utf-8')
start = text.index('uint32_t notify_native(uintptr_t window){')
end = text.index('\nvoid retain(', start)
text = text[:start] + 'uint32_t notify_native(uintptr_t window){return test_native_notify(window);}\n' + text[end:]
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(text, encoding='utf-8', newline='\n')
