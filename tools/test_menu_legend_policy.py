"""Asset-free checks for the legend's shared input policy and binding table."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def test_menu_button_keys_match_the_keycap_labels():
    header = (ROOT / 'port/linux/include/halo_controller_glyphs.h').read_text()
    rows = re.findall(r'X\((\d+), (SDL_SCANCODE_\w+), "([^"]+)"\)', header)
    assert rows == [('0', 'SDL_SCANCODE_RETURN', 'Enter'),
                    ('1', 'SDL_SCANCODE_ESCAPE', 'Escape'),
                    ('2', 'SDL_SCANCODE_E', 'E'),
                    ('3', 'SDL_SCANCODE_TAB', 'Tab')]
    source = (ROOT / 'port/linux/src/xinput_sdl.c').read_text()
    assert 'HALO_GLYPH_MENU_BINDINGS(MENU_SCANCODE)' in source
    assert 'halo_glyph_menu_key(physical)' in source


def test_menu_input_uses_events_instead_of_held_state():
    source = (ROOT / 'port/linux/src/xinput_sdl.c').read_text()
    assert 'void halo_controller_glyph_input_event(' in source
    platform = (ROOT / 'port/linux/src/sdl_platform.c').read_text()
    assert 'halo_controller_glyph_input_event(&event)' in platform
    assert 'halo_glyph_input_note(' in source
    assert 'glyph_menu_input.gamepad' in source
