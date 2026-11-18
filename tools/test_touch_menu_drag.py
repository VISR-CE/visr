"""A finger dragging down a menu list highlights the row under it; the list scrolls in the finger's direction."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WIDGET = (ROOT / 'source/interface/ui_widget.c').read_text()


def test_host_reports_moves_while_dragging(tmp_path):
    binary = tmp_path / 'probe'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-unused-function', '-I', str(ROOT / 'port/ios/host'),
                    '-I', str(ROOT / 'port/linux/src'), '-I', str(ROOT / 'port/runtime/include'),
                    str(ROOT / 'port/ios/tests/menu_pointer_probe.c'), '-o', str(binary)], check=True)
    # the probe's drag case asserts the pointer follows the finger and that a drag is not a tap
    subprocess.run([str(binary)], check=True)


def test_guest_follows_the_finger():
    start = WIDGET.index('if (pointer.wheel_steps && ui_mouse_menu())')
    block = WIDGET[start:start + 1600]
    # a finger over a row leaves the highlight to hover; past the rows the list scrolls with the finger
    assert 'pointer.radius > 0 && back == _widget_event_dpad_up' in block
    assert '_ui_mouse_target_item' in block and 'steps = 0' in block
    assert '(pointer.wheel_steps > 0) != finger_list ? back : forward' in block
    # carousels keep drag scrolling: slot hover is skipped while a finger drags
    assert 'if (ui_mouse_hover_dragging)' in WIDGET
