"""Exercise the widget visibility used by rendering and HUD message suppression."""
from pathlib import Path
import subprocess

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]


def test_hud_menu_visibility_follows_the_player_across_controller_slots(tmp_path):
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    functions = '\n'.join(block(source, marker) for marker in (
        'static boolean ui_widget_should_render(', 'boolean ui_widget_port_screen_visible('))
    for renderer in ('void render_ui_widgets_postgame(', 'void render_ui_widgets('):
        assert 'ui_widget_should_render(widget, widget_index, local_player_index)' in block(source, renderer)
    messaging = (ROOT / 'source/interface/hud_messaging.c').read_text()
    assert '!ui_widget_port_screen_visible(local_player_index)' in block(messaging, 'void hud_messaging_update(')
    program = r'''
#include <assert.h>
#include <stddef.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define PIN(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
struct widget_instance {
    boolean render_regardless_of_controller_index, widget_is_error_dialog;
    short local_player_index;
};
static struct { struct widget_instance *active_widgets[4]; } widget_globals;
static boolean we_are_at_the_main_menu, keyboard;
static boolean virtual_keyboard_active(void) { return keyboard; }
/* PRODUCTION */
int main(void) {
    struct widget_instance pause = {0, 0, 0};
    /* Controller 3 owns player 0's pause menu. */
    widget_globals.active_widgets[3] = &pause;
    assert(ui_widget_port_screen_visible(0));
    assert(!ui_widget_port_screen_visible(1));
    assert(!ui_widget_port_screen_visible(3));
    pause.local_player_index = 1;
    assert(!ui_widget_port_screen_visible(0));
    assert(ui_widget_port_screen_visible(1));
    pause.render_regardless_of_controller_index = TRUE;
    assert(ui_widget_port_screen_visible(0));
    pause.render_regardless_of_controller_index = FALSE;
    pause.widget_is_error_dialog = TRUE;
    pause.local_player_index = NONE;
    assert(ui_widget_port_screen_visible(0));
    assert(ui_widget_port_screen_visible(2));
    pause.local_player_index = 1;
    assert(!ui_widget_port_screen_visible(2));
    we_are_at_the_main_menu = TRUE;
    assert(ui_widget_port_screen_visible(2));
    we_are_at_the_main_menu = FALSE;
    pause.widget_is_error_dialog = FALSE;
    pause.local_player_index = NONE;
    assert(!ui_widget_port_screen_visible(0));
    widget_globals.active_widgets[3] = NULL;
    widget_globals.active_widgets[0] = &pause;
    assert(ui_widget_port_screen_visible(2));
    keyboard = TRUE;
    assert(!ui_widget_port_screen_visible(2));
    keyboard = FALSE;
    widget_globals.active_widgets[0] = NULL;
    assert(!ui_widget_port_screen_visible(0));
    return 0;
}
'''.replace('/* PRODUCTION */', functions)
    probe, binary = tmp_path / 'visibility.c', tmp_path / 'visibility'
    probe.write_text(program)
    subprocess.run(['cc', '-Wall', '-Werror', '-fsanitize=address,undefined', str(probe),
                    '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    import tempfile

    with tempfile.TemporaryDirectory() as directory:
        test_hud_menu_visibility_follows_the_player_across_controller_slots(Path(directory))
