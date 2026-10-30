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
    from harness import build, enum_with, function, read, run, structure
    source = read('port/linux/game/menu_functions.c')
    types = enum_with(source, '_option_long') + structure(source, 'gametype_option')
    import re
    entry = re.search(r'\{ "coop_team_wipe_spinner".*?\n\t\t\{[^}]*\} \}', source, re.S)
    assert entry is not None
    code = 'static struct gametype_option const option = ' + entry.group() + ';\n'
    code += function(source, 'gametype_option_value') + function(source, 'gametype_option_value_set')
    status, output = run(build('coop_wipe_menu', (('types.inc', types), ('under_test.inc', code))), '')
    assert status == 0, output
