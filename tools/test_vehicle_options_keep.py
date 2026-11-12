"""The vehicle options screen keeps its vehicle set as it changes.

The screen's counts were stored live while its set (and the gametype's
universal vehicle set) was stored only on OK or START. Leaving with B kept
the counts but left the set the gametype's own, so Slayer Pro (set: none)
placed no vehicles in the first round. Every change now goes through
`vehicles_keep`, the one place the set is stored.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/linux/game/menu_functions.c'


def function_body(text, name):
    match = re.search(r'^static \w+ ' + name + r'\([^)]*\)\s*\{', text, re.MULTILINE)
    assert match, name
    depth, index = 1, match.end()
    while depth:
        depth += {'{': 1, '}': -1}.get(text[index], 0)
        index += 1
    return text[match.end():index - 1]


def test_update_keeps_the_set_after_count_changes():
    body = function_body(MENUS.read_text(), 'vehicles_update')
    count = body.find('preset->parameters.list.selected_index = VEHICLE_PRESET_CUSTOM;')
    assert count >= 0, body
    # (the side change keeps and returns early; the last keep follows the counts)
    assert body.rfind('vehicles_keep(list);') > count, body


def test_update_keeps_a_preset_only_change():
    body = function_body(MENUS.read_text(), 'vehicles_update')
    tail = body[body.find('VEHICLE_PRESET_CUSTOM;'):]
    assert 'vehicles_preset_index(options, gametype_edit.vehicle_side)' in tail, body
    assert 'vehicles_keep(list);' in tail, body


def test_keep_stores_the_universal_set():
    body = function_body(MENUS.read_text(), 'vehicles_keep')
    assert 'options->vehicle_set[side] =' in body, body
    assert 'variant->universal_variant.vehicle_set =' in body, body


def test_update_keeps_the_respawn_time():
    # (B leaves without OK: the respawn time is kept as it changes, too)
    text = MENUS.read_text()
    body = function_body(text, 'vehicles_update')
    assert 'vehicles_respawn_keep(list);' in body, body
    keep = function_body(text, 'vehicles_respawn_keep')
    assert 'gametype_option_named("vehicles_respawn_spinner")' in keep, keep
    # (only a move off the stored value's place is stored, so a time between
    # the spinner's values is not snapped by opening the screen)
    assert 'gametype_option_index(option, gametype_option_value(option, variant, options))' in keep, keep
    assert 'gametype_option_value_set(option, option->values[index], variant, options);' in keep, keep
