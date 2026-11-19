"""A text box's own text must stay a `widget_memory_pool` block.

`widget_instance_render_text_box` refreshes a string-list text box by calling
`pool_resize_pointer` on `parameters.text_box.text` every render, and the pool halts
the game (`stack_memory_pool_valid_block(pool, reference)`) on any other pointer.
The touch SAVE caption once swapped a static array into that field and halted
ADVANCED CONTROLS on the first tap.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ASSIGNMENT = re.compile(r'parameters\.text_box\.text\s*=(?!=)\s*([^;]*);')
POOL_SOURCES = ('pool_resize_pointer(', 'ui_widget_realloc(')
# the original game's fallback, which the next render's resize also trips on
ORIGINAL = {('source/interface/ui_widget.c', 'L"<out of memory>"')}


def _assignments():
    for folder in ('source', 'port'):
        for path in sorted((ROOT / folder).rglob('*.c')):
            relative = str(path.relative_to(ROOT))
            if '/tests/' in relative or '/third_party/' in relative:
                continue
            text = path.read_text(errors='replace')
            for match in ASSIGNMENT.finditer(text):
                yield relative, match.group(1).strip()


def test_text_box_text_is_only_ever_a_widget_pool_block():
    found = list(_assignments())
    assert found, 'the pattern no longer matches any text box assignment'
    strays = [(path, value) for path, value in found
              if not value.startswith(POOL_SOURCES) and (path, value) not in ORIGINAL]
    assert strays == []


def test_save_caption_draws_through_the_render_override():
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert 'if (relabel) ui_text_box_render_override = save_text;' in source
    body = source.split('static void widget_instance_render_text_box(\n', 2)[2]
    body = body.split('\n}\n', 1)[0]
    assert 'text = override_text ? &override_text : &widget->parameters.text_box.text;' in body
    # search and replace may resize its text, which an override's static array cannot take
    assert '!override_text && search_index < definition->search_and_replace_functions.count' in body
