"""Touch legends on the server browser and Direct IP follow the global-vs-contextual rule."""
import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'port/linux/game/menu_functions.c'
MENUS = ROOT / 'port/assets/menus/xbox'
BROWSER_XML = ['main_menu.multiplayer_type_select.join_game.xml',
               'main_menu.multiplayer_type_select.join_game.compact.xml']
DIRECT_IP_XML = 'main_menu.multiplayer_type_select.direct_ip.xml'


def function_body(name):
    text = SOURCE.read_text()
    match = re.search(r'\n(?:static )?[\w ]+[ *]' + name + r'\([^)]*\)\n\{', text)
    assert match, name
    depth, index = 0, match.end() - 1
    for index in range(match.end() - 1, len(text)):
        if text[index] == '{':
            depth += 1
        elif text[index] == '}':
            depth -= 1
            if depth == 0:
                break
    return text[match.end():index]


def test_browser_touch_hides_back_and_join_and_glyphs():
    body = function_body('browser_touch_legend_update')
    assert 'menu_touch_active(screen)' in body
    assert 'visible_set(back, !touch)' in body
    # JOIN GAME lives in the detail panel, whose update re-shows it, so touch only hides it, after that update
    assert 'if (touch)\n\t\tvisible_set(join, FALSE);' in body
    browser = function_body('browser_update')
    assert browser.index('lobby_browser_update(list);') < browser.index('browser_touch_legend_update(screen_of(list));')
    for name in ('browser_ip_address', 'browser_create_inter'):
        assert f'"{name}_icon", "{name}_caption", touch' in body
    # focus never stays on a widget touch hides
    assert 'browser_c_focus(named(screen, "browser_ip_address", 0))' in body


def test_global_buttons_keep_their_captions_on_touch():
    assert 'icon->visible = !touch' in function_body('touch_legend_pair')
    body = function_body('touch_legend_button')
    assert 'BROWSER_LEGEND_CAPTION_X' in body
    assert re.search(r'#define BROWSER_LEGEND_CAPTION_X 34\b', SOURCE.read_text())


def test_direct_ip_touch_hides_glyph_legends():
    body = function_body('direct_ip_touch_legend_update')
    for name in ('a_butn', 'accept'):
        assert f'"{name}"' in body
    # the B legend stays: the touch layer reads it to draw the BACK chevron (the screen has no B handler)
    for name in ('b_butn', 'cancel'):
        assert f'"{name}"' not in body
    assert 'visible_set(named(screen, names[index], 0), !touch)' in body


def test_updates_are_hooked():
    assert 'browser_touch_legend_update(screen_of(list));' in function_body('browser_update')
    assert 'direct_ip_touch_legend_update(screen_of(list));' in function_body('direct_ip_update')


def test_legend_navigation_skips_hidden_targets():
    body = function_body('browser_legend_navigation')
    assert 'target->visible' in body


def test_touch_uses_controller_glyph_rule():
    body = function_body('menu_touch_active')
    assert 'controller_glyph_hidden(owner, BUTTON_A)' in body


@pytest.mark.parametrize('xml', BROWSER_XML)
def test_browser_widget_names_exist(xml):
    text = (MENUS / xml).read_text()
    for name in ('browser_ip_address_icon', 'browser_ip_address_caption', 'browser_create_inter_icon',
                 'browser_create_inter_caption', 'browser_back', 'join_game_button_join'):
        assert f'/{name}"' in text, name
    # the caption sits BROWSER_LEGEND_CAPTION_X right of its glyph
    for name in ('browser_ip_address', 'browser_create_inter'):
        icon = re.search(rf'widget="[^"]*/{name}_icon" x="(\d+)"', text)
        caption = re.search(rf'widget="[^"]*/{name}_caption" x="(\d+)"', text)
        assert icon and caption, name
        assert int(caption.group(1)) - int(icon.group(1)) == 34, name


def test_direct_ip_widget_names_exist():
    text = (MENUS / DIRECT_IP_XML).read_text()
    for name in ('a_butn', 'accept', 'b_butn', 'cancel'):
        assert f'direct_ip_screen/legend/{name}"' in text, name


FILTERS_XML = 'main_menu.multiplayer_type_select.join_game.filters.xml'
PASSWORD_XML = 'main_menu.multiplayer_type_select.join_game.port.xml'


def test_filters_touch_keeps_reset_and_hides_done():
    body = function_body('browser_filters_touch_legend_update')
    assert '"browser_reset_icon", "browser_reset_caption", touch' in body
    assert 'visible_set(named(screen, "browser_done", 0), !touch)' in body
    assert 'browser_filters_touch_legend_update(screen_of(widget));' in function_body('browser_filters_update')


def test_password_touch_hides_accept_and_back_and_moves_hide_caption():
    body = function_body('password_touch_legend_update')
    for name in ('a_butn', 'accept', 'b_butn', 'cancel'):
        assert f'"{name}"' in body
    assert '"x_butn", "x_hide", PASSWORD_LEGEND_CAPTION_X, touch' in body
    assert 'password_touch_legend_update(screen_of(list));' in function_body('password_screen_update')


def test_filters_widget_names_exist():
    text = (MENUS / FILTERS_XML).read_text()
    for name in ('browser_reset_icon', 'browser_reset_caption', 'browser_done'):
        assert f'/{name}"' in text, name
    icon = re.search(r'widget="[^"]*/browser_reset_icon" x="(\d+)"', text)
    caption = re.search(r'widget="[^"]*/browser_reset_caption" x="(\d+)"', text)
    assert icon and caption
    assert int(caption.group(1)) - int(icon.group(1)) == 34


def test_password_widget_names_exist():
    text = (MENUS / PASSWORD_XML).read_text()
    for name in ('a_butn', 'accept', 'b_butn', 'cancel', 'x_butn', 'x_hide'):
        assert f'password_screen/legend/{name}"' in text, name
    icon = re.search(r'legend/x_butn" x="(\d+)"', text)
    caption = re.search(r'legend/x_hide" x="(\d+)"', text)
    assert icon and caption
    source = SOURCE.read_text()
    offset = re.search(r'#define PASSWORD_LEGEND_CAPTION_X (\d+)', source)
    assert offset and int(caption.group(1)) - int(icon.group(1)) == int(offset.group(1))
