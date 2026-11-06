"""The graphics modes' table (port/linux/src/port_graphics_modes.h) against the settings it replaces."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import profile_check
import settings_schema

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / 'port/linux/src/port_graphics_modes.h'
VALUE = r'(?:"([^"]*)"|NULL)'


def rows():
    text = HEADER.read_text()
    body = text.split('#define GRAPHICS_MODE_SETTINGS(X)', 1)[1].split('\nstruct graphics_mode_setting', 1)[0]
    found = re.findall(rf'X\("([a-z_.0-9]+)", {VALUE}, {VALUE}\)', body)
    assert len(found) == body.count('X('), 'a row the test cannot read'
    return [(name, hardcore or None, classic or None) for name, hardcore, classic in found]


def valid(kind, text):
    if kind == 'boolean':
        return text in ('true', 'false')
    if kind == 'integer':
        return re.fullmatch(r'-?[0-9]+', text) is not None
    if kind == 'number':
        return re.fullmatch(r'-?[0-9]+\.[0-9]+', text) is not None
    return kind == 'string' and '"' not in text


def test_modes_are_ordered_and_enhanced_is_the_default():
    text = HEADER.read_text()
    assert re.search(r'graphics_mode_names\[NUMBER_OF_GRAPHICS_MODES\] = \{ "hardcore", "classic", "enhanced" \}', text)
    setting = settings_schema.config_settings()['display.graphics']
    assert setting['type'] == 'string' and setting['default'] == 'enhanced'
    assert setting['x-environment'] == 'HALO_GRAPHICS'


def test_every_row_names_a_setting_with_a_value_of_its_type():
    settings = settings_schema.config_settings()
    names = [name for name, _, _ in rows()]
    assert len(names) == len(set(names))
    for name, hardcore, classic in rows():
        assert name in settings, f'{name} is in the mode table but is no setting'
        kind = settings[name]['type']
        # Hardcore is the original Xbox: it replaces every setting in the table
        assert hardcore is not None and valid(kind, hardcore), (name, hardcore)
        if classic is not None:
            assert valid(kind, classic), (name, classic)
            # Classic only removes additions, which Hardcore removes too
            assert classic == hardcore, name


def test_the_xbox_picture_size_and_rate():
    table = {name: hardcore for name, hardcore, _ in rows()}
    assert table['display.screen_width'] == '640' and table['display.render_height'] == '480'
    assert table['display.render_scale'] == '1.0' and table['display.resolution_scaling'] == 'original'
    assert table['display.frame_rate'] == '30' and table['display.interpolation'] == 'false'
    assert table['display.dynamic_resolution'] == 'false' and table['display.anti_aliasing'] == 'off'


def test_profiles_may_not_set_the_mode(tmp_path):
    assert 'display.graphics' not in profile_check.profile_keys()
    path = tmp_path / 'mode.toml'
    path.write_text('schema=1\nname="Mode"\nkind="model"\n[match]\nplatform="ios"\nmodels=["iPhone18,1"]\n'
                    '[display]\ngraphics="classic"\n')
    errors = profile_check.check_profile(path, profile_check.profile_keys())[0]
    assert any('display.graphics' in error for error in errors)
