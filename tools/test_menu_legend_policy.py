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
    assert 'SDL_GetGamepadFromID(shown.gamepad)' in source


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
    header = (ROOT / 'source/interface/controller_glyphs.h').read_text()
    assert 'raised.y0 -= MENU_BAR_GLYPH_RAISE;' in header
    assert 'controller_glyph_legend_raise(&bounds)' in source
    assert 'controller_glyph_legend_raise(clip)' in source
    assert '&glyph_bounds, clip ? &glyph_clip : NULL, glyph_color, _controller_glyph_blend_menu' in source


def test_mouse_legend_lights_the_acting_button_and_touch_hides_select_and_back():
    source = (ROOT / 'port/linux/src/xinput_sdl.c').read_text()
    assert '"Mouse Left"' in source and '"Mouse Right"' in source
    widget = (ROOT / 'source/interface/ui_widget.c').read_text()
    assert 'ui_touch_legend_suppressed' in widget
    assert 'ui_touch_legend_caption_suppressed' in widget
    glyphs = (ROOT / 'port/ios/host/host_controller_glyphs.m').read_text()
    assert 'glyph_draw_mouse' in glyphs


def test_the_keyboard_secondary_legend_names_backspace_whatever_chose_the_family():
    """Bug class (Nov 25): the DELETE legend showed E on keyboard captures, because the Backspace label was keyed on the last input
    event while the capture tools force the family. The label reads the shown input (`glyph_input_shown`, which honors the
    capture override) and names the one label the header defines."""
    source = (ROOT / 'port/linux/src/xinput_sdl.c').read_text()
    header = (ROOT / 'port/linux/include/halo_controller_glyphs.h').read_text()
    assert '#define HALO_GLYPH_MENU_SECONDARY_LABEL "Backspace"' in header
    assert 'glyph_menus_active && shown.source == HALO_GLYPH_INPUT_KEYBOARD && physical == 2 && glyph_x_destructive' in source
    assert 'snprintf(label, size, "%s", HALO_GLYPH_MENU_SECONDARY_LABEL)' in source
    assert 'glyph_menu_input.source == HALO_GLYPH_INPUT_KEYBOARD && physical == 2' not in source


def test_the_destructive_x_flag_is_declared_outside_the_ios_guard():
    """Bug class (Nov 25): `glyph_x_destructive` was declared inside `#ifdef HALO_IOS` while `keyboard_gamepad` (every build) reads it,
    and nothing but a Linux build saw it. A variable a function outside a guard uses must be declared outside it."""
    lines = (ROOT / 'port/linux/src/xinput_sdl.c').read_text().splitlines()
    depth, declared_at = 0, None
    for number, line in enumerate(lines, 1):
        text = line.strip()
        if text.startswith(('#if', '#ifdef', '#ifndef')):
            depth += 1
        elif text.startswith('#endif'):
            depth -= 1
        elif text.startswith('static int glyph_x_destructive'):
            declared_at = (number, depth)
    assert declared_at and declared_at[1] == 0, declared_at
