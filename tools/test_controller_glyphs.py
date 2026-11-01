"""Controller prompt policy at the input/renderer boundary."""
from pathlib import Path
import subprocess
import tempfile
import shutil
import re

ROOT = Path(__file__).resolve().parents[1]


def block(source, marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def blend_enums():
    source = (ROOT / 'source/rasterizer/xbox/rasterizer_xbox.c').read_text()
    return next(item.group() for item in re.finditer(r'enum\s*\{[^}]*\};', source)
                if 'NUMBER_OF_SHADER_FRAMEBUFFER_BLEND_FUNCTIONS' in item.group())


def glyph_blends():
    source = (ROOT / 'source/interface/controller_glyphs.h').read_text()
    return re.search(r'enum\s*\{[^}]*\};', source).group()


def test_touch_captions_and_initial_keyboard_owner(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    widget = (ROOT / 'source/interface/ui_widget.c').read_text()
    keyboard = (ROOT / 'source/interface/virtual_keyboard.c').read_text()
    cases = {
        'caption': ('controller_glyph_caption_probe.c', {
            '/* CAPTION */': block(widget, 'static wchar_t const *ui_widget_touch_caption(')}),
        'owner': ('controller_glyph_owner_probe.c', {
            '/* OWNER */': block(keyboard, 'void virtual_keyboard_set_controller_context('),
            '/* LAUNCH */': block(keyboard, 'boolean virtual_keyboard_launch(')}),
    }
    for name, (fixture, replacements) in cases.items():
        program = (ROOT / 'port/ios/tests' / fixture).read_text()
        for marker, production in replacements.items():
            program = program.replace(marker, production)
        source, binary = tmp_path / f'{name}.c', tmp_path / name
        source.write_text(program)
        subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                        str(source), '--output=' + str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


def test_virtual_keyboard_fallback_and_legend_patches(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    source = (ROOT / 'source/interface/virtual_keyboard.c').read_text()
    function = block(source, 'static void virtual_keyboard_draw_background(')
    program = (ROOT / 'port/ios/tests/controller_glyph_keyboard_probe.c').read_text().replace('/* BACKGROUND */', function).replace('/* GLYPH BLENDS */', glyph_blends())
    probe, binary = tmp_path / 'keyboard.c', tmp_path / 'keyboard'
    probe.write_text(program)
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_native_controller_bridge_mapping(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    source = (ROOT / 'port/ios/host/host_sdl.c').read_text()
    function = block(source, 'int host_sdl_gamepad_glyph(')
    program = (ROOT / 'port/ios/tests/controller_glyph_bridge_probe.c').read_text().replace('/* BRIDGE */', function)
    probe, binary = tmp_path / 'bridge.c', tmp_path / 'bridge'
    probe.write_text(program)
    flags = subprocess.check_output(['pkg-config', '--cflags', 'sdl3'], text=True).split()
    for tv in (0, 1):
        subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined', *flags,
                        f'-DGLYPH_TEST_TV={tv}', '-iquote', str(ROOT / 'port/linux/include'), '-iquote', str(ROOT / 'source'), str(probe),
                        '--output=' + str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


def test_hud_glyph_layout_color_and_fallback(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    source = (ROOT / 'source/interface/hud_messaging.c').read_text()
    source = source[source.rindex('static void render_state_bitmap('):]
    function = block(source, 'static void render_state_bitmap(')
    program = (ROOT / 'port/ios/tests/controller_glyph_hud_probe.c').read_text().replace('/* HUD */', function).replace('/* GLYPH BLENDS */', glyph_blends()).replace('/* RENDERER BLENDS */', blend_enums())
    probe, binary = tmp_path / 'hud.c', tmp_path / 'hud'
    probe.write_text(program)
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_controller_glyph_draw_cache_and_clipping(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('#ifdef HALO_IOS\n#define CONTROLLER_GLYPH_CACHE_SIZE')
    end = source.index('/* ---------- public code */', start)
    program = (ROOT / 'port/ios/tests/controller_glyph_draw_probe.c').read_text().replace('/* DRAW */', source[start:end])
    hud = (ROOT / 'source/interface/hud_draw.c').read_text()
    stock = block(hud[hud.rindex('static void hud_draw_bitmap_internal('):], 'static void hud_draw_bitmap_internal(')
    expression = re.search(r'parameters\.framebuffer_blend_function\s*=\s*(\w+);', stock).group(1)
    program = program.replace('/* RENDERER BLENDS */', blend_enums()).replace('/* STOCK BLEND */',
        f'static int stock_hud_blend(void) {{ return {expression}; }}')
    renderer = (ROOT / 'source/rasterizer/xbox/rasterizer_xbox.c').read_text()
    tables = '\n'.join(re.search(r'static const unsigned long ' + name + r'\[[^]]*\]\s*=\s*\{.*?\};',
        renderer, re.S).group() for name in ('srcblend_table', 'destblend_table'))
    factors = 'enum { D3DBLEND_ZERO, D3DBLEND_ONE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA, D3DBLEND_DESTCOLOR, D3DBLEND_SRCCOLOR };'
    shader = (ROOT / 'source/rasterizer/xbox/rasterizer_xbox_dynavobgeom.c').read_text()
    final = next(item for item in re.findall(r'pixel_shader\.final_combiner_inputs_abcd\s*=\s*([^;]+);', shader)
                 if 'parameters->alpha_weighted' in item)
    alpha = re.findall(r'pixel_shader\.final_combiner_inputs_efg\s*=\s*([^;]+);', shader)[-1]
    program = program.replace('/* BLEND TABLES */', factors + tables).replace('/* FINAL COMBINER */',
        'static unsigned long glyph_final_combiner(struct rasterizer_dynamic_screen_geometry_parameters *parameters) '
        f'{{ return {final}; }}\n#define GLYPH_FINAL_ALPHA ({alpha})')
    probe, binary = tmp_path / 'draw.c', tmp_path / 'draw'
    probe.write_text(program)
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    '-iquote', str(ROOT / 'port/linux/include'), '-iquote', str(ROOT / 'source'), str(probe),
                    '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_controller_glyph_requests(tmp_path):
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    source = (ROOT / 'port/linux/src/xinput_sdl.c').read_text()
    function = block(source, 'int halo_controller_glyph_request(')
    program = (ROOT / 'port/ios/tests/controller_glyph_request_probe.c').read_text().replace('/* REQUEST */', function)
    first = source.index('static const char *const binding_settings[')
    settings = source[first:source.index('};', first)+2]
    program = program.replace('/* BINDING SETTINGS */', settings).replace('/* BINDINGS READ */', block(source, 'static void bindings_read(void)'))
    activity = '\n'.join(block(source, marker) for marker in ('void halo_controller_glyph_pointer_activity(', 'static void glyph_keyboard_activity(', 'static int glyph_gamepad_active('))
    program = program.replace('/* ACTIVITY */', activity)
    probe, binary = tmp_path / 'request.c', tmp_path / 'request'
    probe.write_text(program)
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-Wno-unused-variable',
                    '-fsanitize=address,undefined', '-iquote', str(ROOT / 'port/linux/include'),
                    '-iquote', str(ROOT / 'port/runtime/include'), '-iquote', str(ROOT / 'port/linux/src'),
                    *subprocess.check_output(['pkg-config', '--cflags', 'sdl3'], text=True).split(),
                    str(probe), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_controller_glyph_policy(tmp_path):
    # Wrong semantic bindings, shared-player state, or losing shoulder/axis
    # identity must change the request the renderer receives.
    if not shutil.which('xcrun'):
        import pytest
        pytest.skip('Apple compiler and frameworks are required')
    program = r'''
#include <assert.h>
#include <string.h>
#include "halo_controller_glyphs.h"
int main(void) {
 assert(halo_glyph_keyboard_action(18) == 13); /* action, independent of X/reload */
 assert(halo_glyph_keyboard_action(19) == 7);
 assert(halo_glyph_keyboard_action(20) == 6);
 assert(halo_glyph_keyboard_action(22) == 4);
 assert(halo_glyph_keyboard_action(26) == 5);
 assert(halo_glyph_keyboard_action(27) == 10);
 assert(halo_glyph_keyboard_action(100) == -1);
 assert(halo_glyph_element(4) == 10); /* Black -> right shoulder */
 assert(halo_glyph_element(5) == 9); /* White -> left shoulder */
 assert(halo_glyph_element(6) == 4); /* trigger axis */
 assert(halo_glyph_element(7) == 5);
 assert(halo_glyph_is_axis(6) && halo_glyph_is_axis(7));
 assert(halo_glyph_is_axis(16) && halo_glyph_is_axis(17));
 assert(!halo_glyph_is_axis(4));
 assert(halo_glyph_element(-1) == -1 && halo_glyph_element(18) == -1);
 return 0;
}
'''
    probe = tmp_path / 'glyphs.c'
    probe.write_text(program)
    binary = tmp_path / 'glyphs'
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    '-iquote', str(ROOT / 'port/linux/include'), '-iquote', str(ROOT / 'source'), str(probe),
                    '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    with tempfile.TemporaryDirectory() as directory:
        test_controller_glyph_policy(Path(directory))
        test_controller_glyph_requests(Path(directory))
        test_controller_glyph_draw_cache_and_clipping(Path(directory))
        test_hud_glyph_layout_color_and_fallback(Path(directory))
        test_native_controller_bridge_mapping(Path(directory))
        test_virtual_keyboard_fallback_and_legend_patches(Path(directory))
        test_touch_captions_and_initial_keyboard_owner(Path(directory))
