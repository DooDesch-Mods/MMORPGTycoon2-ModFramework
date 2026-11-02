"""Compile the actual dispatch and ordering bodies with bounded native test doubles."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
dispatch = (args.source / 'determinism_loading_dispatch.inc').read_text(encoding='utf-8')
dispatch = dispatch[:dispatch.index('void install(')] + '}\n'
ordered = (args.source / 'determinism_ordered_finish.inc').read_text(encoding='utf-8')
ordered = ordered[:ordered.index('void install(')] + '}\n'
stubs = {
    '#include "determinism_ordered_finish.inc"': ordered,
    '#include "determinism_ordered_run.inc"': '''namespace ordered_run {
void begin(uintptr_t){} void end(uintptr_t){} void before_unlock(void*,uintptr_t){}
void cancel(){} Json status(){return nullptr;}
}''',
    '#include "native_save_completion.inc"': '''namespace save_completion {
void* current=nullptr;
void before_unlock(void*){throw std::runtime_error("unexpected Save completion scope");}
}''',
    '#include "native_save_cache_completion.inc"': '''namespace save_cache {
void* current=nullptr;
void before_unlock(void*){throw std::runtime_error("unexpected Save cache scope");}
}''',
}
for anchor, replacement in stubs.items():
    assert dispatch.count(anchor) == 1, anchor
    dispatch = dispatch.replace(anchor, replacement)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(dispatch, encoding='utf-8', newline='\n')
