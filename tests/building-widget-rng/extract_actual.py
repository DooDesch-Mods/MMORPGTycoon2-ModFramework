"""Extract the production widget owner, native wrapper and vector dispatcher."""
from pathlib import Path
import sys

source = Path(sys.argv[1]) / 'adapters/mt2'
output = Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
parts = (
    ('native_step_runtime.inc', 'bool building_widget_owner(const void* building,', '\nuintptr_t tasks(){', 'widget_owner_actual.inc'),
    ('determinism_presentation_rng.inc', '__declspec(noinline) void* get_pad_entrance(', '\nvoid verify(', 'widget_wrapper_actual.inc'),
    ('determinism_rng_vector_trace.inc', 'using Vector3Box=', '\nusing Color=', 'widget_vector_actual.inc'),
)
for filename, start, end, target in parts:
    text = (source / filename).read_text(encoding='utf-8')
    if text.count(start) != 1 or text.count(end) != 1:
        raise RuntimeError('Widget fixture extraction markers differ: ' + filename)
    first = text.index(start)
    last = text.index(end, first)
    (output / target).write_text(text[first:last] + '\n', encoding='utf-8', newline='\n')
