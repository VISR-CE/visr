"""Co-op rules use a dedicated options page in both menu looks."""
import sys
from pathlib import Path
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import port_settings
import xbox_menus

ROOT = Path(__file__).resolve().parents[3]
SETUP = 'main_menu.multiplayer_type_select.server_settings.xml'
NAME = 'main_menu.multiplayer_type_select.coop_options.xml'
BASE = 'main_menu/multiplayer_type_select/coop_options/'
SETUP_BASE = 'main_menu/multiplayer_type_select/server_settings/'


def test_fireteam_generated_screen():
    text = '\n'.join(port_settings.multiplayer_files()[NAME])
    root = ET.fromstring(text)
    widgets = {w.get('name'): w for w in root.findall('widget')}
    spinner = widgets[BASE + 'fireteam_spinner']
    assert spinner.get('strings').split('|') == ['DROP-IN', 'CLASSIC']
    assert spinner.get('setting') == 'network.coop_late_join'
    assert spinner.get('values') == 'true|false'
    rows = {r.get('widget').removeprefix(BASE): r for r in widgets[BASE + 'options_menu'].findall('child')}
    positions = {'op_friendly_fire': 73, 'op_extra_enemies': 103,
                 'op_enemies_per_player': 133, 'op_enemies_multiplier': 133,
                 'op_player_collisions': 163, 'op_ally_friendly_fire': 193,
                 'op_fireteam': 223}
    for name, y in positions.items():
        assert int(rows[name].get('y')) == y
    row = rows['op_fireteam']
    help_top = int(widgets[BASE + 'help'].get('top'))
    assert int(row.get('y')) + int(widgets[BASE + 'op_fireteam'].get('height')) < help_top
    assert (ROOT / 'port/assets/menus/ce' / NAME).read_text() == text


def test_fireteam_xbox_generated_screen():
    assets = ROOT / 'port/assets/menus'
    generated = xbox_menus.multiplayer_documents(assets, set())
    xbox_menus.native_header_sizes(generated, assets)
    assert (assets / 'xbox' / NAME).read_text() == xbox_menus.serialize(generated[NAME])


def test_fireteam_setting_binding():
    from harness import build, function, read, run, structure
    source = read('port/linux/game/menu_functions.c')
    root = ET.fromstring('\n'.join(port_settings.multiplayer_files()[NAME]))
    spinner = root.find(f"widget[@name='{BASE}fireteam_spinner']")
    assert spinner.find("on[@event='created']").get('run') == 'port setting load'
    assert spinner.find("on[@event='deleted']") is None
    values = spinner.get('values').split('|')
    import json
    types = structure(source, 'pc_menu_setting')
    metadata = 'static struct pc_menu_setting setting = {1, ' + json.dumps(spinner.get('setting'))
    metadata += ', ' + str(len(values)) + ', {' + ','.join(map(json.dumps, values)) + '}, NONE};\n'
    code = ''.join(function(source, name) for name in
                   ('text_is_number', 'spinner_item', 'setting_value_index', 'setting_load', 'setting_changed_save'))
    status, output = run(build('fireteam_menu', (('types.inc', types), ('metadata.inc', metadata),
                                                ('under_test.inc', code))), '')
    assert status == 0, output


def test_fireteam_help_has_two_lines():
    root = ET.fromstring('\n'.join(port_settings.multiplayer_files()[SETUP]))
    strings = root.find(f"strings[@name='{SETUP_BASE}help_strings']").findall('string')
    expected = [
        ['Players can leave and join at any time.', 'Players always respawn in place upon death.'],
        ['Fireteam cannot change after level start.', 'If the whole team dies, revert to checkpoint.'],
    ]
    for item, words in zip(strings[29:31], expected, strict=True):
        lines = item.get('text').split('\\n')
        assert lines == words
        assert len(lines) == 2
        assert max(map(len, lines)) <= 49


def test_coop_page_navigation_and_hosting_rows():
    for look in ('ce', 'xbox'):
        prefix = 'xbox/' if look == 'xbox' else ''
        setup_root = ET.parse(ROOT / 'port/assets/menus' / look / SETUP).getroot()
        page_root = ET.parse(ROOT / 'port/assets/menus' / look / NAME).getroot()
        setup = {w.get('name'): w for w in setup_root.findall('widget')}
        page = {w.get('name'): w for w in page_root.findall('widget')}
        base = prefix + BASE
        setup_base = prefix + SETUP_BASE
        row = setup[setup_base + 'op_coop_options']
        for event in ('a', 'start'):
            assert row.find(f"on[@event='{event}']").get('open') == base + 'coop_options_screen'
        assert row.find("on[@event='left_mouse']").get('run') == 'mouse emit accept event'
        screen = page[base + 'coop_options_screen']
        for event in ('b', 'back'):
            assert screen.find(f"on[@event='{event}']").get('back') == 'true'
        for event in ('a', 'start'):
            accept = page[base + 'button_ok'].find(f"on[@event='{event}']")
            assert accept.get('back') == 'true'
            assert accept.get('run') == 'coop options save'
        rows = {c.get('widget').split('/')[-1]: c for c in setup[setup_base + 'options_menu'].findall('child')}
        assert not any(key in rows for key in ('op_friendly_fire', 'op_extra_enemies', 'op_fireteam'))
        assert int(rows['op_map_share'].get('y')) == (223 if look == 'xbox' else 193)
        assert rows['op_coop_options'].get('y') == rows['op_game_type'].get('y')
        assert list(rows).index('op_map_share') < list(rows).index('op_coop_options')
        for key, _ in port_settings.SETUP_OPTION_SCREENS:
            assert setup[setup_base + 'op_' + key].find("on[@event='a']").get('run') == 'port setup edit'
        for spinner in page.values():
            if spinner.get('setting'):
                assert spinner.find("on[@event='created']").get('run') == 'port setting load'
                assert spinner.find("on[@event='deleted']") is None
                assert spinner.find("on[@event='left_mouse']").get('run') == 'mouse spinner 1wide click'


def test_coop_choices_and_dependent_rows():
    from harness import build, function, read, run
    source = read('port/linux/game/menu_functions.c')
    constants = source[source.index('#define COOPERATIVE_FRIENDLY_FIRE_HELP'):source.index('/* the most players Server Setup shows')]
    status, output = run(build('coop_options_menu', (('constants.inc', constants),
                         ('update.inc', function(source, 'coop_options_update') + '\n' + function(source, 'server_coop_rules_apply')))), '')
    assert status == 0, output
