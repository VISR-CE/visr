"""Run the menu dialog routing and debug launch hooks without game data."""
from pathlib import Path
import shutil
import os
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_dialog_hooks_in_development_and_shipping_builds(tmp_path):
    compiler = shutil.which('cc')
    if not compiler:
        pytest.skip('C compiler unavailable')
    source = (ROOT / 'port/linux/game/menu_tags.c').read_text()
    hooks = source[source.index('static boolean menu_delete_from_select;'):
                   source.index('char const *pc_menus_root_name(\n')]
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "menu_equivalence.h"
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define UI_WIDGET_DEFINITION_TAG 0
static struct { int loaded, xbox_set; } menu_tags = {1, 1};
static char const *tag_name, *setting = "";
static int calls, abort_calls, actual_code, actual_flag;
static char const *actual_variant;
static int menu_use_xbox(void) { return 1; }
static int platform_menu_keyboard(void) { return 0; }
static void menu_profile_description(int unused) {}
static char const *tag_get_name(long tag) { return tag_name; }
static long tag_loaded(int group, char const *name) { return 2; }
static void platform_log(char const *format, ...) {}
static char const *config_string(char const *name) { return setting; }
char const *pc_menu_debug_error_variant(char const *name, boolean modal);
static void display_error(short code, short player, boolean modal, boolean pause) {
    calls++; actual_code = code; actual_flag = modal;
    actual_variant = pc_menu_debug_error_variant("full", modal);
}
static void display_error_abort_to_dashboard(short code, boolean optional) {
    abort_calls++; actual_code = code; actual_flag = optional;
}
'''
    checks = r'''
int main(void) {
    tag_name = "ui\\shell\\error\\error_modal";
    assert(pc_menu_screen_open(1, NULL, FALSE) == 1);
    assert(pc_menu_screen_open(1, NULL, TRUE) == 2);
    assert(pc_menu_screen_open(NONE, "ui\\shell\\error\\error_modal_halfscreen", TRUE) == 2);
    tag_name = "ui\\shell\\error\\warning_creating_player_profile_screen";
    assert(pc_menu_screen_open(1, NULL, FALSE) == 2);
    tag_name = "ui\\shell\\error\\confirm_delete_profile_modal_fullscreen";
    pc_menu_screen_open(1, NULL, TRUE);
    assert(pc_menu_delete_from_select());
    tag_name = "pc\\xbox\\error\\confirm_delete_profile_modal_fullscreen";
    pc_menu_screen_open(1, NULL, TRUE);
    assert(!pc_menu_delete_from_select());
    tag_name = "ui\\shell\\error\\confirm_delete_profile_modal_fullscreen";
    pc_menu_screen_open(1, NULL, TRUE);
    tag_name = "pc\\xbox\\error\\confirm_delete_gametype_modal_fullscreen";
    pc_menu_screen_open(1, NULL, TRUE);
    assert(!pc_menu_delete_from_select());
    char const *valid[] = {"12,1,full", "7,0,half", "13,1,qtr", "33,1,abort", "35,0,abort"};
    for (int i = 0; i < 5; i++) {
        setting = valid[i]; menu_error_used = FALSE;
        menu_debug_error_schedule();
        int before = calls + abort_calls;
        assert(menu_error_pending);
        pc_menu_debug_error_ready();
        assert(calls + abort_calls == before + 1);
        pc_menu_debug_error_ready();
        menu_debug_error_schedule();
        assert(!menu_error_pending);
        assert(calls + abort_calls == before + 1);
        assert(!strcmp(pc_menu_debug_error_variant("unchanged", 1), "unchanged"));
        if (i == 1) assert(strstr(actual_variant, "nonmodal_halfscreen"));
        if (i == 2) assert(strstr(actual_variant, "modal_qtrscreen"));
        if (i == 3) assert(actual_code == 33 && actual_flag == 1);
        if (i == 4) assert(actual_code == 35 && actual_flag == 0);
    }
    char const *invalid[] = {"", "-1,1,full", "40,1,full", "7,2,half", "7,0,nope", "7,0,qtr,garbage"};
    for (int i = 0; i < 6; i++) {
        setting = invalid[i]; menu_error_used = FALSE;
        menu_debug_error_schedule();
        assert(!menu_error_pending);
    }
    return 0;
}
'''
    shutil.copy(ROOT / 'port/linux/include/menu_equivalence.h', tmp_path)
    path = tmp_path / 'hooks.c'
    path.write_text(harness + hooks + checks)
    for defines in ([],):
        binary = tmp_path / 'hooks'
        subprocess.run([compiler, '-std=c99', '-I', str(tmp_path),
                        *defines, str(path), '-o', str(binary)], check=True, capture_output=True)
        subprocess.run([str(binary)], check=True, capture_output=True,
                       env={**os.environ, 'HALO_HOST_TVOS': '1'})


def test_dialog_legend_tracks_input_changes_without_reopening(tmp_path):
    compiler = shutil.which('cc')
    if not compiler:
        pytest.skip('C compiler unavailable')
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    body = source[source.index('static void dialog_legend_update('):
                  source.index('/* "3wide player profile list update":')]
    program = r'''
#include <assert.h>
#include <string.h>
typedef int boolean;
#define BUTTON_A 0
#define NONE -1
static short local_player_index_for_draw_string_and_hack_in_icons = 2;
struct widget_instance { char const *name; short local_player_index; int visible;
    struct widget_instance *child, *next; };
static int hidden;
static int controller_glyph_hidden(short player, short physical) {
    assert(player == 2 && physical == BUTTON_A); return hidden;
}
/* BODY */
int main(void) {
    struct widget_instance caption = {"=accept", 2, 0, 0, 0};
    struct widget_instance glyph = {"a_butn_glyph", 2, 0, 0, &caption};
    struct widget_instance touch_caption = {"touch_caption", 2, 0, 0, &glyph};
    struct widget_instance background = {"touch_background", 2, 0, 0, &touch_caption};
    struct widget_instance target = {"a_butn", 2, 1, &background, 0};
    for (int i = 0; i < 6; i++) {
        target.local_player_index = i < 3 ? 2 : NONE;
        hidden = i % 3 == 1;
        dialog_legend_update(&target);
        assert(background.visible == hidden && touch_caption.visible == hidden);
        assert(glyph.visible == !hidden && caption.visible == !hidden);
        assert(target.visible);
    }
}
'''.replace('/* BODY */', body)
    path, binary = tmp_path / 'legend.c', tmp_path / 'legend'
    path.write_text(program)
    subprocess.run([compiler, '-std=c99', str(path), '-o', str(binary)],
                   check=True, capture_output=True)
    subprocess.run([str(binary)], check=True, capture_output=True)
