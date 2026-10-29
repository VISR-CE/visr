"""Input-class policy and generated screen pairs, without game data."""
import importlib.util
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_equivalence_generation():
    spec = importlib.util.spec_from_file_location('xbox_menus', ROOT / 'tools/xbox_menus.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    assert module.equivalence_header() == (ROOT / 'port/linux/include/menu_equivalence.h').read_text()
    pairs = module.screen_pairs()
    assert len({name for pair in pairs for name in pair}) == len(pairs) * 2
    assert not any('campaign_select' in name or 'player_profiles_select' in name for pair in pairs for name in pair)


def test_input_and_screen_policy(tmp_path):
    source = tmp_path / 'probe.c'
    source.write_text(r'''
#include <assert.h>
#include "menu_input.h"
#include "menu_equivalence.h"
int main(void) {
    struct menu_input_tracker state = {0};
    char const *screen = menu_screen_pairs[0][1];
    menu_input_start(&state, 0, 0, 0);
    assert(!state.pad);
    menu_input_activity(&state, 1, 0, 0, 0);
    assert(state.pad);
    /* Activity does not replace the open screen. The next open resolves it. */
    assert(!strcmp(screen, menu_screen_pairs[0][1]));
    screen = menu_screen_equivalent(screen, state.pad);
    assert(!strcmp(screen, menu_screen_pairs[0][0]));
    menu_input_activity(&state, 0, 3, 0, 1);
    assert(state.pad);
    menu_input_activity(&state, 0, 4, 0, 1);
    assert(!state.pad);
    assert(!strcmp(screen, menu_screen_pairs[0][0]));
    screen = menu_screen_equivalent(screen, state.pad);
    assert(!strcmp(screen, menu_screen_pairs[0][1]));
    assert(!menu_screen_equivalent("unpaired", 0));
    char const *warning = "pc\\error\\confirm_join_big_modal_fullscreen";
    char const *xbox_warning = "pc\\xbox\\error\\confirm_join_big_modal_fullscreen";
    assert(!strcmp(menu_screen_equivalent(warning, 1), xbox_warning));
    assert(!strcmp(menu_screen_equivalent(xbox_warning, 0), warning));
    menu_input_start(&state, 1, 0, 0);
    assert(state.pad);
    menu_input_activity(&state, 0, 0, 0, 0);
    assert(!state.pad);
    menu_input_start(&state, 0, 0, 1);
    assert(state.pad);
    menu_input_start(&state, 1, 1, 0);
    menu_input_activity(&state, 0, 0, 0, 0);
    assert(state.pad);
    for (unsigned int i = 0; i < sizeof(menu_screen_pairs)/sizeof(menu_screen_pairs[0]); i++) {
        assert(!strcmp(menu_screen_equivalent(menu_screen_pairs[i][0], 0), menu_screen_pairs[i][1]));
        assert(!strcmp(menu_screen_equivalent(menu_screen_pairs[i][1], 1), menu_screen_pairs[i][0]));
    }
    return 0;
}
''')
    binary = tmp_path / 'probe'
    subprocess.run([shutil.which('cc'), '-std=c11', '-Dxbox=1', '-Wall', '-Werror', '-Wno-unused-function',
                    '--include-directory=' + str(ROOT / 'port/linux/src'),
                    '--include-directory=' + str(ROOT / 'port/linux/include'), str(source), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
