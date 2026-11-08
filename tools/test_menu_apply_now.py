"""Exercise menu persistence with the production handlers and storage boundaries."""
from pathlib import Path
import subprocess
import shutil

import pytest

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]


def test_settings_and_bindings_persist_without_save(tmp_path):
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    required = ('static boolean profile_controls_save(', 'static boolean setting_apply_now(')
    assert all(signature in source for signature in required), 'immediate persistence handlers are missing'
    functions = '\n'.join(block(source, signature) for signature in (
        'static boolean setting_changed_save(', *required, 'static boolean setting_default_show('))
    controls = source[source.index('#define CONTROL_BINDINGS'):source.index('static struct widget_instance *control_row(')]
    controls += '\n' + block(source, 'static void control_capture_poll(')
    fixture = (ROOT / 'port/ios/tests/menu_apply_now_probe.c').read_text()
    program = fixture.replace('/* SETTINGS */', functions).replace('/* CONTROLS */', controls)
    probe, binary = tmp_path / 'probe.c', tmp_path / 'probe'
    probe.write_text(program)
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the menu persistence probe')
    subprocess.run([compiler, '-std=c99', '-Wall', '-Werror', '-Wno-unused-function',
                    '-fsanitize=address,undefined', str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
