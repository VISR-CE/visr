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
                    str(ROOT / 'port/ios/tests/menu_pointer_probe.c'), '-o', str(binary), '-lm'], check=True)
    # the probe's drag case asserts the pointer follows the finger and that a drag is not a tap
    subprocess.run([str(binary)], check=True)


def test_guest_follows_the_finger():
    start = WIDGET.index('if (pointer.wheel_steps && ui_mouse_menu())')
    block = WIDGET[start:start + 1600]
    # rows and horizontal lists share one direction rule with the dropdown (halo_ui_list_wheel_steps, probed in menu_drag_probe)
    assert 'halo_ui_list_wheel_steps(pointer.wheel_steps, finger, over_row)' in block
    assert '_ui_mouse_target_item' in block
    # carousels keep drag scrolling: slot hover is skipped while a finger drags
    assert 'if (ui_mouse_hover_dragging)' in WIDGET


def test_list_rule_probe(tmp_path):
    binary = tmp_path / 'drag'
    subprocess.run([shutil.which('cc'), '-Wall', '-Werror', '-Wno-unused-function', '-I', str(ROOT / 'port/ios/host'),
                    '-I', str(ROOT / 'port/linux/src'), '-I', str(ROOT / 'port/runtime/include'),
                    str(ROOT / 'port/ios/tests/menu_drag_probe.c'), '-o', str(binary), '-lm'], check=True)
    subprocess.run([str(binary)], check=True)
