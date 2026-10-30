"""TEAM WIPE extends VISR's flat Server Setup without moving existing rows."""
import sys
from pathlib import Path
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import port_settings
import xbox_menus

ROOT = Path(__file__).resolve().parents[3]
NAME = 'main_menu.multiplayer_type_select.server_settings.xml'
BASE = 'main_menu/multiplayer_type_select/server_settings/'


def test_team_wipe_generated_screen():
    text = '\n'.join(port_settings.multiplayer_files()[NAME])
    root = ET.fromstring(text)
    widgets = {w.get('name'): w for w in root.findall('widget')}
    spinner = widgets[BASE + 'coop_team_wipe_spinner']
    assert spinner.get('strings').split('|') == ['AUTO', 'REVERT CHECKPOINT', 'RESPAWN ONLY']
    assert spinner.get('setting') == 'network.coop_team_wipe'
    assert spinner.get('values') == 'auto|revert|respawn'
    rows = {r.get('widget').removeprefix(BASE): r for r in widgets[BASE + 'options_menu'].findall('child')}
    positions = {'op_friendly_fire': 217, 'op_extra_enemies': 241,
                 'op_enemies_per_player': 265, 'op_enemies_multiplier': 265,
                 'op_player_collisions': 289, 'op_ally_friendly_fire': 313, 'op_map_share': 193}
    for name, y in positions.items():
        assert int(rows[name].get('y')) == y
    row = rows['op_coop_team_wipe']
    assert int(row.get('y')) == 337
    help_top = int(widgets[BASE + 'help'].get('top'))
    assert int(row.get('y')) + int(widgets[BASE + 'op_coop_team_wipe'].get('height')) < help_top
    assert (ROOT / 'port/assets/menus/ce' / NAME).read_text() == text


def test_team_wipe_xbox_generated_screen():
    assets = ROOT / 'port/assets/menus'
    generated = xbox_menus.multiplayer_documents(assets, set())
    xbox_menus.native_header_sizes(generated, assets)
    assert (assets / 'xbox' / NAME).read_text() == xbox_menus.serialize(generated[NAME])


def test_team_wipe_setting_binding():
    from harness import build, function, read, run, structure
    source = read('port/linux/game/menu_functions.c')
    root = ET.fromstring('\n'.join(port_settings.multiplayer_files()[NAME]))
    spinner = root.find(f"widget[@name='{BASE}coop_team_wipe_spinner']")
    assert spinner.find("on[@event='created']").get('run') == 'port setting load'
    assert spinner.find("on[@event='deleted']").get('run') == 'port setting save'
    values = spinner.get('values').split('|')
    import json
    types = structure(source, 'pc_menu_setting')
    metadata = 'static struct pc_menu_setting setting = {1, ' + json.dumps(spinner.get('setting'))
    metadata += ', ' + str(len(values)) + ', {' + ','.join(map(json.dumps, values)) + '}, NONE};\n'
    code = ''.join(function(source, name) for name in
                   ('text_is_number', 'spinner_item', 'setting_value_index', 'setting_load', 'setting_save'))
    status, output = run(build('coop_wipe_menu', (('types.inc', types), ('metadata.inc', metadata),
                                                ('under_test.inc', code))), '')
    assert status == 0, output


def test_team_wipe_help_has_two_lines():
    root = ET.fromstring('\n'.join(port_settings.multiplayer_files()[NAME]))
    strings = root.find(f"strings[@name='{BASE}help_strings']").findall('string')
    for item in strings[29:32]:
        lines = item.get('text').split('\\n')
        assert len(lines) == 2
        assert max(map(len, lines)) <= 49
