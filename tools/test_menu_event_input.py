"""Exercise the SDL event pump's press counting without a window."""

from pathlib import Path
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("scancode,mod,counts", [
    ("SDL_SCANCODE_LGUI", "0", 0),
    ("SDL_SCANCODE_LSHIFT", "0", 0),
    ("SDL_SCANCODE_RCTRL", "0", 0),
    ("SDL_SCANCODE_CAPSLOCK", "0", 0),
    ("SDL_SCANCODE_M", "SDL_KMOD_GUI", 0),
    ("SDL_SCANCODE_M", "SDL_KMOD_CTRL | SDL_KMOD_GUI", 0),
    ("SDL_SCANCODE_3", "SDL_KMOD_SHIFT | SDL_KMOD_GUI", 0),
    ("SDL_SCANCODE_A", "SDL_KMOD_ALT", 0),
    ("SDL_SCANCODE_A", "0", 1),
    ("SDL_SCANCODE_A", "SDL_KMOD_SHIFT", 1),
])
def test_intro_skip_ignores_modifiers_and_chords(tmp_path, scancode, mod, counts):
    source = (ROOT / "port/linux/src/sdl_platform.c").read_text()
    helper = "static int key_counts_as_press" + source.split(
        "static int key_counts_as_press", 1)[1].split("\nunsigned int platform_input_press_count", 1)[0]
    program = tmp_path / "chord.c"
    program.write_text(r'''
#include <assert.h>
enum { SDL_SCANCODE_A = 4, SDL_SCANCODE_M = 16, SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_LALT, SDL_SCANCODE_LGUI,
    SDL_SCANCODE_RCTRL, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RALT, SDL_SCANCODE_RGUI,
    SDL_SCANCODE_MODE, SDL_SCANCODE_CAPSLOCK = 57 };
#define SDL_KMOD_SHIFT 0x3u
#define SDL_KMOD_CTRL 0xc0u
#define SDL_KMOD_ALT 0x300u
#define SDL_KMOD_GUI 0xc00u
''' + helper + r'''
int main(void) {
    assert(key_counts_as_press(''' + scancode + ", " + mod + r''') == ''' + str(counts) + r''');
    return 0;
}
''')
    binary = tmp_path / "chord"
    subprocess.run([shutil.which("cc"), "-std=c11", "-Wall", "-Werror", str(program),
                    "--output=" + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
