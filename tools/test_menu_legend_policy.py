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
    assert 'HALO_GLYPH_MENU_WILDCARD_SCANCODE SDL_SCANCODE_R' in header
    assert 'case 14: return "R";' in header
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


def test_keyboard_symbols_cover_menu_keys_and_special_composition():
    header = (ROOT / 'port/linux/include/halo_keyboard_symbols.h').read_text()
    for label, symbol in [('Enter', 'return'), ('Escape', 'escape'), ('Tab', 'arrow.right.to.line'),
                          ('Shift', 'shift'), ('Command', 'command'), ('Alt', 'option'),
                          ('Ctrl', 'control'), ('Backspace', 'delete.left'), ('Space', 'space'),
                          ('Up', 'arrow.up'), ('Down', 'arrow.down'), ('Left', 'arrow.left'), ('Right', 'arrow.right')]:
        assert f'{{"{label}", "{symbol}"}}' in header
    assert 'halo_keyboard_symbol_plan(' in header
    raster = (ROOT / 'port/ios/host/host_controller_glyphs.m').read_text()
    assert 'halo_keyboard_symbol_plan(' in raster
    assert 'missing SF Symbol' in raster
    assert 'bezierPathWithRoundedRect' not in raster
    assert 'drawAtPoint:' not in raster


def test_original_legend_strings_use_port_overrides():
    header = (ROOT / 'port/linux/include/menu_legend_strings.h').read_text()
    assert 'halo_menu_legend_plain(' in header
    assert 'L"\\t%b-button quit    %a-button pick game"' in header
    assert 'L"\\t%b-button quit"' in header
    source = (ROOT / 'source/text/text_group.c').read_text()
    assert 'halo_menu_legend_plain(result)' in source
    assert 'halo_menu_legend_fallback(' in source


def test_virtual_keyboard_separator_patch_preserves_the_typed_key():
    source = (ROOT / 'source/interface/virtual_keyboard.c').read_text()
    assert 'rectangle2d separators[2] = {{414,397,440,408}, {414,498,440,509}};' in source
    # Patches touch only the legend strip, not the grid of typed characters.
    assert 'controller_glyph_blit(bitmap, &separators[index], &sample' in source


def test_legend_glyphs_center_on_caption_cap_height():
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert '#define LEGEND_GLYPH_RAISE 3' in source
    assert 'glyph_bounds.y0 -= LEGEND_GLYPH_RAISE;' in source
    assert '&glyph_bounds, clip, glyph_color, _controller_glyph_blend_menu' in source
