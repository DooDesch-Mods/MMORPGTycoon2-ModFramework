"""Extract the actual read-only state body into the nearest controlled-memory fixture."""
from pathlib import Path
import hashlib
import json
import re
import sys

source,output=map(Path,sys.argv[1:3])
text=source.read_text(encoding="utf-8")
start=text.index("Json probe_wizard_state(")
opening=text.index("{",start)
depth=0
for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]',text[opening:]):
    if token.group()=="{": depth+=1
    elif token.group()=="}":
        depth-=1
        if depth==0:
            body=text[start:opening+token.end()]
            break
else: raise RuntimeError("Read-only wizard state body is unclosed")
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(body+"\n",encoding="utf-8")
output.with_suffix(".json").write_text(json.dumps({"source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),"body_sha256":hashlib.sha256(body.encode()).hexdigest()},indent=2)+"\n",encoding="utf-8")
