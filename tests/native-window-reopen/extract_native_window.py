from pathlib import Path
import argparse
import hashlib

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
text = args.source.read_text(encoding="utf-8")
begin = text.index('    Json events{{', text.index('Json native_window('))
end = text.index('    for (auto& item : items)', begin)
body = text[begin:end]
expected = "dcff2b12f052ba88744b7fbaf12ab60859df335ea9bd9ca683e9dd92d21717b6"
actual = hashlib.sha256(body.encode("utf-8")).hexdigest()
if actual != expected:
    raise RuntimeError(f"Native window block differs: expected {expected}, actual {actual}")
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(body, encoding="utf-8", newline="\n")
