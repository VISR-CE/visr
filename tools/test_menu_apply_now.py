"""Exercise menu persistence with the production handlers and storage boundaries."""
from pathlib import Path
import subprocess
import shutil

import pytest

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]


def test_settings_and_bindings_persist_without_save(tmp_path):
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    required = ('static boolean profile_save_now(', 'static boolean setting_apply_now(')
    assert all(signature in source for signature in required), 'immediate persistence handlers are missing'
    functions = '\n'.join(block(source, signature) for signature in (
        'static boolean setting_changed_save(', *required, 'static boolean setting_default_show(',
        'boolean pc_menu_profile_auto_save_enabled(', 'boolean pc_menu_profile_save_on_back(',
        'void pc_menu_profile_name_saved(', 'static boolean color_choose('))
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


def test_native_controller_steps_save_through_the_original_handler(tmp_path):
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    helper = block(source, 'static void widget_profile_spinner_save(')
    for direction in ('next', 'previous'):
        handler = block(source, f'boolean widget_event_function_list_widget_goto_{direction}_item(')
        assert 'widget_profile_spinner_save(widget)' in handler
    program = r'''
#include <assert.h>
#include <stddef.h>
#define TRUE 1
#define FALSE 0
#define _ui_widget_type_spinner_list 2
#define _event_handler_run_function_bit 1
#define TEST_FLAG(value, bit) ((value) & (1 << (bit)))
typedef int boolean;
struct ui_widget_event_handler_reference { int flags, function; };
struct ui_widget_definition { struct { int count; void *address; } event_handlers; };
struct widget_instance { int type, definition_tag_index; struct widget_instance *parent; };
static int active = 1, enabled = 1, calls, saves, last_function;
static struct ui_widget_event_handler_reference handler = { 2, 70 };
static struct ui_widget_definition definitions[2] = { { { 0, 0 } }, { { 1, &handler } } };
static int main_menu_is_active(void) { return active; }
boolean pc_menu_profile_auto_save_enabled(void) { return enabled; }
boolean pc_menu_profile_save_on_back(void) { saves++; return 1; }
static struct ui_widget_definition *ui_widget_definition_get(int index) { return &definitions[index]; }
static int ui_widget_event_handler_function_invoke(struct widget_instance *list, void *event, int function, int *deleted)
{ assert(list->definition_tag_index == 1); calls++; last_function = function; return 1; }
/* HELPER */
int main(void) {
    struct widget_instance list = { 3, 1, 0 }, row = { 0, 0, &list }, spinner = { 2, 0, &row };
    widget_profile_spinner_save(&spinner);
    assert(calls == 1 && saves == 1 && last_function == 70);
    handler.function = 71;
    widget_profile_spinner_save(&spinner);
    assert(calls == 2 && saves == 2 && last_function == 71);
    handler.function = 60; /* Gametype save is excluded. */
    widget_profile_spinner_save(&spinner);
    assert(calls == 2 && saves == 2);
    handler.function = 70; enabled = 0;
    widget_profile_spinner_save(&spinner);
    enabled = 1; active = 0;
    widget_profile_spinner_save(&spinner);
    assert(calls == 2 && saves == 2);
    return 0;
}
'''
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the native controller probe')
    probe, binary = tmp_path / 'native.c', tmp_path / 'native'
    probe.write_text(program.replace('/* HELPER */', helper))
    subprocess.run([compiler, '-std=c99', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
