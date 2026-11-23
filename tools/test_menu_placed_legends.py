"""The placed legend glyphs (`*_butn` widgets) only go away.

A screen's button bar is declared (`<bar>`, placed by the engine in the standard slots) or, on the screens not yet
converted (dialogs, the lobby, the browser, FILTERS and the action-bar screens), drawn from glyph widgets the
generator places by hand. Each placed glyph is one more place a slot's x can drift from the standard slots, so the
count may fall as screens move to `<bar>` and may never rise; lower CEILING when it falls.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CEILING = 65


def placed_legend_glyphs():
    found = []
    for path in sorted((ROOT / 'port/assets/menus/xbox').glob('*.xml')):
        found += [(path.name, widget.get('name')) for widget in ET.parse(path).getroot().iter('widget')
                  if widget.get('name', '').endswith('_butn')]
    return found


def test_placed_legend_glyphs_never_grow():
    count = len(placed_legend_glyphs())
    assert count <= CEILING, (f'{count} placed legend glyphs, more than {CEILING}: declare the new bar with '
                              '`bar()` / `legend()` instead of placing glyph widgets')


def test_the_ceiling_follows_the_count_down():
    count = len(placed_legend_glyphs())
    assert count == CEILING, f'{count} placed legend glyphs now: lower CEILING to {count}'


def test_touch_draws_any_fifth_button_in_the_b_slot():
    """On touch, B's slot is empty (BACK is the title's chevron), so a bar's fifth button (the lobby's SHARE on the
    left stick) is drawn there, wherever its glyph is placed for a controller: run `ui_touch_legend_slot_for_button`."""
    import shutil
    import subprocess
    import tempfile
    import pytest

    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the slot probe')
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('static short ui_touch_legend_slot_for_button(')
    end = source.index('\n}\n', start) + 3
    program = ('#include <assert.h>\n#define NONE (-1)\n'
               'enum { _gamepad_analog_button_a, _gamepad_analog_button_b, _gamepad_analog_button_x,\n'
               '       _gamepad_analog_button_y, _gamepad_binary_button_left_thumb = 14 };\n'
               + source[start:end] +
               'int main(void) {\n'
               '    assert(ui_touch_legend_slot_for_button(_gamepad_analog_button_a) == 0);\n'
               '    assert(ui_touch_legend_slot_for_button(_gamepad_analog_button_b) == 1);\n'
               '    assert(ui_touch_legend_slot_for_button(_gamepad_analog_button_y) == 2);\n'
               '    assert(ui_touch_legend_slot_for_button(_gamepad_analog_button_x) == 3);\n'
               '    assert(ui_touch_legend_slot_for_button(_gamepad_binary_button_left_thumb) == 1);\n'
               '    return 0;\n}\n')
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / 'probe.c'
        probe.write_text(program)
        subprocess.run([compiler, '-Wall', '-Werror', str(probe), '-o', str(probe.with_suffix(''))], check=True)
        subprocess.run([str(probe.with_suffix(''))], check=True)
