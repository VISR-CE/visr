"""Keep the developer label's prior PC behavior while showing VISR's Xbox version."""
from pathlib import Path
import subprocess

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]


def test_build_label_by_menu_set(tmp_path):
    source = (ROOT / 'source/interface/ui_widget_game_data_input_functions.c').read_text()
    function = block(source, 'static void set_textbox_to_build_number(\n\tstruct widget_instance *widget)\n{')
    program = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define csmemset memset
#define csstrcmp strcmp
#define ustrncpy wcsncpy
struct widget_instance { struct { struct { wchar_t *text; } text_box; } parameters; };
static const char *menus = "pc";
static const char *config_string(const char *key) { return menus; }
static void pc_menu_version_text(char *text, long size) { strcpy(text, "VISR 0.7.0"); }
static void ascii_to_wide(const char *text, wchar_t *wide, long size)
{ mbstowcs(wide, text, size / sizeof(*wide)); }
static void *ui_widget_realloc(void *p, long size, const char *file, long line) { return realloc(p, size); }
/* FUNCTION */
int main(void)
{
    struct widget_instance widget = { 0 };
    set_textbox_to_build_number(&widget);
#ifdef HALO_IOS
    assert(!widget.parameters.text_box.text[0]);
    menus = "xbox";
    set_textbox_to_build_number(&widget);
    assert(!wcscmp(widget.parameters.text_box.text, L"VISR 0.7.0"));
    menus = "pc";
    set_textbox_to_build_number(&widget);
    assert(!widget.parameters.text_box.text[0]);
#else
    assert(!wcscmp(widget.parameters.text_box.text, L"01.01.14.2342"));
#endif
    free(widget.parameters.text_box.text);
}
'''.replace('/* FUNCTION */', function)
    path, binary = tmp_path / 'label.c', tmp_path / 'label'
    path.write_text(program)
    for flags in (['-DHALO_IOS=1'], []):
        subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-Wno-unused-function',
                        '-fsanitize=address,undefined', *flags, str(path), '--output=' + str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
