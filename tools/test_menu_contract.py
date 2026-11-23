"""The names the menu XML and the menu code share resolve on both sides.

`menu_tags.c` fails the whole menu set (the game's own menus then show) when an XML file names a function
it does not know, and `menu_functions.c` finds widgets by leaf name and does nothing when one is missing. These
tests read the name tables out of the C sources and compare them with the shipped XML, so a rename on either side
fails here and not on a device.
"""
from pathlib import Path
import re
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
XBOX = ROOT / 'port/assets/menus/xbox'
TAGS = (ROOT / 'port/linux/game/menu_tags.c').read_text(errors='replace')
FUNCTIONS = (ROOT / 'port/linux/game/menu_functions.c').read_text(errors='replace')
HANDLERS = (ROOT / 'source/interface/ui_widget_event_handler_functions.c').read_text(errors='replace')

# Leaf names `menu_functions.c` looks up that the Xbox-look XML does not define, with the reason each is allowed.
# Remove an entry when its lookup goes or its widget arrives; nothing new belongs here.
LOOKUPS_OUTSIDE_THE_XBOX_XML = {}


def table(source, declaration):
    """The string literals of a `static char const *const NAME[] = { ... };` table."""
    start = source.index(declaration)
    return re.findall(r'"((?:[^"\\]|\\.)*)"', source[start:source.index('};', start)])


def xml_roots():
    return {path.name: ET.parse(path).getroot() for path in sorted(XBOX.glob('*.xml'))}


def test_every_function_the_xml_runs_is_one_the_tag_builder_knows():
    port = set(table(TAGS, 'static char const *const port_function_names[] ='))
    game = set(table(HANDLERS, 'event_handler_function_list ='))
    unknown = []
    for filename, root in xml_roots().items():
        for element in root.iter():
            for key in ('run', 'created'):
                name = element.get(key)
                if name and name not in port | game:
                    unknown.append((filename, element.tag, key, name))
    assert unknown == []


def test_every_game_data_input_the_xml_names_is_one_the_tag_builder_knows():
    port = set(table(TAGS, 'static char const *const port_game_data_input_names[] ='))
    game = set(table(TAGS, 'static char const *const game_data_input_names[] ='))
    unknown = [(filename, data.get('input')) for filename, root in xml_roots().items() for data in root.iter('data')
               if data.get('input') not in port | game]
    assert unknown == []


def test_every_port_function_the_xml_runs_is_dispatched():
    """A `port ...` function listed for the tag builder but never compared by name in menu_functions.c runs nothing."""
    dispatched = set(re.findall(r'!strcmp\(name, "((?:[^"\\]|\\.)*)"\)', FUNCTIONS))
    used = set()
    for root in xml_roots().values():
        for element in root.iter():
            used.update(element.get(key) for key in ('run', 'created') if element.get(key))
            if element.tag == 'data':
                used.add(element.get('input'))
    # (spinner settings load, save and change through the settings table, not the name chain)
    missing = sorted(name for name in used if name.startswith('port ') and name not in dispatched
                     and not name.startswith('port setting '))
    assert missing == []


def test_every_widget_the_menu_code_looks_up_is_defined():
    leaves = set()
    for root in xml_roots().values():
        leaves.update(widget.get('name', '').rsplit('/', 1)[-1] for widget in root.iter('widget'))
        leaves.update(row.get('name') for row in root.iter('row') if row.get('name'))
    looked_up = set(re.findall(r'\bnamed\((?:[^,()]|\([^()]*\))+, "([^"]+)"', FUNCTIONS))
    assert len(looked_up) > 100, 'the lookup pattern no longer matches menu_functions.c'
    missing = looked_up - leaves
    assert missing == set(LOOKUPS_OUTSIDE_THE_XBOX_XML), sorted(missing ^ set(LOOKUPS_OUTSIDE_THE_XBOX_XML))


def test_every_widget_and_string_list_reference_resolves():
    widgets, strings, bitmaps = set(), set(), set()
    roots = xml_roots()
    for root in roots.values():
        widgets.update(widget.get('name') for widget in root.iter('widget'))
        strings.update(entry.get('name') for entry in root.iter('strings'))
        bitmaps.update(entry.get('name') for entry in root.iter('bitmap'))
    dangling = []
    for filename, root in roots.items():
        for element in root.iter():
            if element.tag == 'row':
                continue  # a patch row's fields name the map's tags and text, checked by the patch tests
            for key, known in (('widget', widgets), ('description', widgets), ('string_list', strings),
                               ('bitmap', bitmaps), ('header_bitmap', bitmaps), ('footer_bitmap', bitmaps)):
                name = element.get(key)
                if name and '\\' not in name and name not in known:
                    dangling.append((filename, element.tag, key, name))
            for key in ('open', 'replace', 'focus', 'otherwise'):
                if element.tag == 'action' and key == 'focus':
                    continue  # a bar action's focus="true": the focused widget takes the button
                name = element.get(key)
                if name and '\\' not in name and name not in widgets:
                    dangling.append((filename, element.tag, key, name))
    assert dangling == []
