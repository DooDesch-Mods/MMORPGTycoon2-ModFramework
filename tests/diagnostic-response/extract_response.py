"""Extract production response retention and dispatch control flow for fault tests."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding='utf-8')
start = source.index('bool publish_diagnostic_response(){')
end = source.index('void update(void* game,float dt)', start)
body = source[start:end]
dispatch_start = body.index('        if(op=="status")result=status();')
dispatch_end = body.index('        response["ok"]=true;', dispatch_start)
body = body[:dispatch_start] + '        result=execute(op,args);\n' + body[dispatch_end:]
destination = Path(sys.argv[2])
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(body, encoding='utf-8')
