from pathlib import Path
import sys
source=Path(sys.argv[1])/'adapters/mt2/determinism_rng_trace.inc';text=source.read_text(encoding='utf-8')
start='__declspec(noinline) int get_int(void* source,int limit){';end='\n__declspec(noinline) int get_int_range('
assert text.count(start)==text.count(end)==1
Path(sys.argv[2]).write_text(text[text.index(start):text.index(end)]+'\n',encoding='utf-8')
