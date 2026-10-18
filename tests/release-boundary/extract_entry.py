"""Extract the actual physical Mode.Command entry for focused Windows checks."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
start = source.index("DWORD command_view(")
end = source.index("\nbool prepare_save()", start)
body = source[start:end]
prefix = "__declspec(noinline) void ingame_command(void* mode,const void* command) {\n    const auto native_return=reinterpret_cast<uintptr_t>(_ReturnAddress());"
if prefix not in body:
    raise RuntimeError("Physical noinline Mode entry must capture its immediate return before helper calls")
if source.count("hook(ingame,reinterpret_cast<void*>(ingame_command)") != 1:
    raise RuntimeError("Expected exactly one physical Framework Mode.Command hook")
output = Path(sys.argv[2])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(body + "\n", encoding="utf-8")
