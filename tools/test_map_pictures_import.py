"""Asset-free disc-import map picture ordering, state, and persisted-cache regressions."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_native_map_pictures_import(tmp_path):
    game = ROOT / "port/linux/game"
    executable = tmp_path / "map-pictures-import"
    subprocess.run([
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-DMAP_PICTURES_IMPORT_TEST",
        "-I", str(game), str(game / "map_pictures_import_test.c"),
        str(game / "map_pictures_scenario.c"), str(game / "map_pictures.c"),
        str(game / "map_pictures_image.c"), str(game / "bmp_files.c"),
        "-lm", "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable), str(tmp_path)], check=True)


def test_host_import_contract():
    names = ("host_map_pictures_import_stage", "host_map_pictures_import_progress",
             "host_map_pictures_import_done", "host_map_pictures_begin_refresh",
             "host_map_framings_feed_path")
    imports = (ROOT / "port/runtime/host_imports.list").read_text().splitlines()
    header = (ROOT / "port/runtime/guest/runtime/guest_host.h").read_text()
    for name in names:
        assert imports.count(name) == 1
        assert name + "(" in header
    host = (ROOT / "port/ios/host/host_data_updates.m").read_text()
    assert "int host_map_framings_feed_path(" in host
    assert "host_data_updates_set_bundled_framings_sequence([" in host


def test_scripted_import_capture_gate(tmp_path):
    source = (ROOT / "port/linux/game/photo_mode.c").read_text()
    update = source[source.index("void photo_mode_update("):]
    start = update.index("if ((!host_photo_mode_available()") + len("if (")
    end = update.index("\n\t{", start)
    condition = update[start:end].strip()[:-1]  # Remove the if statement's closing parenthesis.
    probe = tmp_path / "photo-capture-gate.c"
    probe.write_text(r'''
#include <assert.h>
static int available, importing, scripted, debug;
static int host_photo_mode_available(void) { return available; }
static int host_map_pictures_import_stage(void) { return importing; }
/* fake: the probe switches the debug setting per case */
static int config_boolean(const char *key) { (void)key; return debug; }
static int refused(void) { return CONDITION; }
int main(void)
{
    int bits;
    for (bits = 0; bits < 16; bits++) {
        available = bits & 1; importing = (bits >> 1) & 1;
        scripted = (bits >> 2) & 1; debug = (bits >> 3) & 1;
        assert((!refused()) == (scripted ? (available || importing) : (available && debug)));
    }
    return 0;
}
'''.replace("CONDITION", condition))
    executable = tmp_path / "photo-capture-gate"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                    str(probe), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
