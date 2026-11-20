"""Generated screen counterparts, without game data."""
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


def test_screen_counterparts(tmp_path):
    source = tmp_path / 'probe.c'
    source.write_text(r'''
#include <assert.h>
#include "menu_equivalence.h"
int main(void) {
    assert(!menu_screen_equivalent("unpaired"));
    char const *warning = "pc\\error\\confirm_join_big_modal_fullscreen";
    char const *xbox_warning = "pc\\xbox\\error\\confirm_join_big_modal_fullscreen";
    assert(!strcmp(menu_screen_equivalent(warning), xbox_warning));
    assert(!strcmp(menu_screen_equivalent(xbox_warning), xbox_warning));
    /* a screen asked for by a pair's second name opens its first */
    for (unsigned int i = 0; i < sizeof(menu_screen_pairs)/sizeof(menu_screen_pairs[0]); i++) {
        assert(menu_screen_equivalent(menu_screen_pairs[i][0]));
        assert(!strcmp(menu_screen_equivalent(menu_screen_pairs[i][1]), menu_screen_pairs[i][0]));
    }
    /* a replacement that always applies wins over a pair */
    for (unsigned int i = 0; i < sizeof(menu_screen_always)/sizeof(menu_screen_always[0]); i++)
        assert(!strcmp(menu_screen_equivalent(menu_screen_always[i][0]), menu_screen_always[i][1]));
    return 0;
}
''')
    binary = tmp_path / 'probe'
    subprocess.run([shutil.which('cc'), '-std=c11', '-Dxbox=1', '-Wall', '-Werror', '-Wno-unused-function',
                    '--include-directory=' + str(ROOT / 'port/linux/include'), str(source), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
