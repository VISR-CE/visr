"""A row that opens a screen of its own shows its setting's state word (NETWORK's MAPS: ON or OFF).

Bug class: a stateful row's word kept in a function of its own per row (respawn, teams and HUD) or left stale
until the screen is rebuilt. "port setting state" is one function for every such row: the text widget names its
setting, strings and values (tools/port_settings.py `opens`), and the word shown is the string of the setting's
value now. This runs the production functions of port/linux/game/menu_functions.c against a stub widget and
settings table, and checks the generated rows the function serves."""
from pathlib import Path
import subprocess
from xml.etree import ElementTree as ET

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]
MENUS = ROOT / 'port/assets/menus/xbox'

PROGRAM = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define _stricmp strcasecmp
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define MAXIMUM_STRINGS 64
struct widget_instance
{
    long definition_tag_index;
    union { struct { short string_list_index; } text_box; } parameters;
};
struct pc_menu_setting { long definition_index; char const *setting; long value_count;
                         char const *values[MAXIMUM_STRINGS]; short loaded_index; };
static struct pc_menu_setting entry = { 7, "network.custom_maps", 2, { "true", "false" }, NONE };
struct pc_menu_setting *pc_menu_setting_get(long definition_index) { return definition_index == 7 ? &entry : NULL; }
/* the game's settings: what config.toml holds now */
static char const *stored = "false";
static boolean setting_text(char const *name, char *text, unsigned int size, boolean default_value)
{
    (void)default_value;
    if (strcmp(name, "network.custom_maps")) return FALSE;
    snprintf(text, size, "%s", stored);
    return TRUE;
}
/* PRODUCTION */
TEXT_IS_NUMBER
SETTING_VALUE_INDEX
SETTING_STATE_SHOW

int main(void)
{
    struct widget_instance on_off = { 7, { { -1 } } }, stranger = { 9, { { 5 } } };

    stored = "true";
    setting_state_show(&on_off);
    assert(on_off.parameters.text_box.string_list_index == 0);
    /* (the player turns it off in the MAPS screen and comes back) */
    stored = "false";
    setting_state_show(&on_off);
    assert(on_off.parameters.text_box.string_list_index == 1);
    /* (a text that names no setting is left as it is) */
    setting_state_show(&stranger);
    assert(stranger.parameters.text_box.string_list_index == 5);
    return 0;
}
'''


def test_setting_state_shows_the_word_of_the_value_now(tmp_path):
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    program = (PROGRAM
               .replace('TEXT_IS_NUMBER', block(source, 'static boolean text_is_number('))
               .replace('SETTING_VALUE_INDEX', block(source, 'static short setting_value_index('))
               .replace('SETTING_STATE_SHOW', block(source, 'static void setting_state_show(')))
    (tmp_path / 'probe.c').write_text(program)
    subprocess.run(['clang', '-std=gnu11', '-Wall', '-Werror', '-Wno-unused-function', str(tmp_path / 'probe.c'),
                    '-o', str(tmp_path / 'probe')], check=True)
    subprocess.run([str(tmp_path / 'probe')], check=True)


def test_state_rows_are_generated_from_the_data_the_function_reads():
    """Every text with a setting is one the function serves: strings and values of equal length, the function as its
    data input, and the row around it opens a screen that exists."""
    screens = {w.get('name') for path in MENUS.glob('settings.*.xml') for w in ET.parse(path).iter('widget')}
    found = []
    for path in MENUS.glob('*.xml'):
        root = ET.parse(path).getroot()
        widgets = {w.get('name'): w for w in root.iter('widget')}
        for widget in root.iter('widget'):
            if widget.get('type') != 'text' or widget.get('setting') is None:
                continue
            found.append(widget.get('setting'))
            assert widget.find('data').get('input') == 'port setting state', widget.get('name')
            assert len(widget.get('strings').split('|')) == len(widget.get('values').split('|')), widget.get('name')
            rows = [w for w in widgets.values() if any(c.get('widget') == widget.get('name')
                                                       for c in w.findall('child'))]
            assert len(rows) == 1, widget.get('name')
            opens = {on.get('open') for on in rows[0].findall('on') if on.get('event') in ('a', 'start')}
            assert len(opens) == 1 and opens <= screens, (widget.get('name'), opens)
    assert found == ['network.custom_maps']


def test_network_keeps_six_rows_and_maps_holds_the_map_sharing_rows():
    def rows(page):
        root = ET.parse(MENUS / f'settings.{page}.xml').getroot()
        options = next(w for w in root.iter('widget') if w.get('name', '').endswith(f'{page}/options_menu'))
        return [c.get('widget').rsplit('/', 1)[-1] for c in options.findall('child')
                if c.get('widget').rsplit('/', 1)[-1].startswith('op_')]

    # (one platform shows CHECK FOR UPDATES and another ICLOUD SAVES, in the same place)
    network = rows('network_setup')
    assert network == ['op_online', 'op_join_from_clipboard', 'op_auto', 'op_cloud_saves', 'op_player_names',
                       'op_custom_maps', 'op_enabled']
    assert rows('maps') == ['op_custom_maps', 'op_map_download', 'op_map_seed']
    # (the settings NETWORK no longer shows keep their names and values in config.toml)
    cut = {'network.allow_upnp', 'network.map_seed_upload', 'display.player_name_scale',
           'display.scoreboard_team_layout', 'display.scoreboard_background'}
    shown = {w.get('setting') for path in MENUS.glob('settings.*.xml') for w in ET.parse(path).iter('widget')}
    assert not cut & shown
    config = (ROOT / 'port/linux/src/port_config.c').read_text()
    for name in cut:
        entry = config[config.index(f'\t{{ "{name}",'):]
        assert '_tier_tuning' in entry[:entry.index('\n\t{ "', 5)], name
