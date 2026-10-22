from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
begin = source.index("bool native_input_flag(")
end = source.index("bool command(void* receiver", begin)
output = Path(sys.argv[2])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(source[begin:end], encoding="utf-8", newline="\n")
