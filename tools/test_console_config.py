"""The script console's `visr_set SECTION.KEY VALUE` (port/linux/src/console_config.c): development builds set a
setting for this run through config_set_live, distribution builds refuse and change nothing, and any other line
still goes to the script compiler. config_set_live itself runs against the real settings resolver in
port/ios/tests/config_live_probe.c (the class: a set that reports success without changing the value in force)."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r"""
#include <stdio.h>
#include <string.h>
#include "console_config.h"

static char last_name[128], last_value[300];
static int calls;

int config_set_live(const char *name, const char *value, char *note, size_t note_size)
{
	calls++;
	snprintf(last_name, sizeof(last_name), "%s", name);
	snprintf(last_value, sizeof(last_value), "%s", value);
	if (!strcmp(name, "display.no_such_setting"))
	{
		snprintf(note, note_size, "no setting %s", name);
		return 0;
	}
	snprintf(note, note_size, "%s = %s", name, value);
	return 1;
}

int main(int argc, char **argv)
{
	char reply[256] = "";
	int handled;

	(void)argc;
	handled = console_config_command(argv[1], reply, sizeof(reply));
	printf("%d|%d|%s|%s|%s\n", handled, calls, last_name, last_value, reply);
	return 0;
}
"""


def build(tmp_path):
    harness = tmp_path / "harness.c"
    harness.write_text(HARNESS)
    executable = tmp_path / "console-config"
    # (only the header: port/linux/include also holds the guest's own stdio.h and friends)
    include = tmp_path / "include"
    include.mkdir()
    (include / "console_config.h").write_text((ROOT / "port/linux/include/console_config.h").read_text())
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(include),
                    str(harness), str(ROOT / "port/linux/src/console_config.c"), "-o", str(executable)], check=True)
    return executable


def run(executable, line, hooks):
    environment = dict(os.environ, HALO_DEVELOPMENT_HOOKS="1" if hooks else "0")
    output = subprocess.run([str(executable), line], env=environment, capture_output=True, text=True, check=True)
    handled, calls, name, value, reply = output.stdout.rstrip("\n").split("|", 4)
    return int(handled), int(calls), name, value, reply


def test_development_build_sets_settings(tmp_path):
    executable = build(tmp_path)
    cases = {
        "visr_set display.shadow_map_size 256": ("display.shadow_map_size", "256"),
        "  visr_set display.shadow_map_size=512": ("display.shadow_map_size", "512"),
        "visr_set display.shadow_map_size = 128  ": ("display.shadow_map_size", "128"),
        'visr_set display.anti_aliasing "smaa"': ("display.anti_aliasing", "smaa"),
        'visr_set display.graphics="classic"': ("display.graphics", "classic"),
        "visr_set display.soft_particles true": ("display.soft_particles", "true"),
    }
    for line, (name, value) in cases.items():
        assert run(executable, line, hooks=True) == (1, 1, name, value, f"visr_set: {name} = {value}"), line


def test_development_build_reports_bad_lines(tmp_path):
    executable = build(tmp_path)
    assert run(executable, "visr_set display.no_such_setting 1", hooks=True)[0:2] == (1, 1)
    assert run(executable, "visr_set display.no_such_setting 1", hooks=True)[4] == \
        "visr_set: refused: no setting display.no_such_setting"
    for line in ("visr_set", "visr_set display.shadow_map_size", "visr_set display.shadow_map_size   "):
        handled, calls, _, _, reply = run(executable, line, hooks=True)
        assert (handled, calls) == (1, 0), line
        assert reply.startswith("visr_set: usage:"), line


def test_distribution_build_refuses(tmp_path):
    executable = build(tmp_path)
    assert run(executable, "visr_set display.shadow_map_size 256", hooks=False) == (
        1, 0, "", "", "visr_set: not available in this build")


def test_other_lines_go_to_the_script_compiler(tmp_path):
    executable = build(tmp_path)
    for line in ("(game_speed 0)", "visr_settings 1", "game_speed 0", "", "visr_setx display.shadow_map_size 1"):
        for hooks in (True, False):
            assert run(executable, line, hooks)[0:2] == (0, 0), (line, hooks)


def test_console_checks_the_command_before_compiling_a_script():
    source = (ROOT / "source/networking/telnet_console.c").read_text()
    body = source[source.index("static boolean process_telnet_client_buffer(\n\tchar"):]
    command = body.index("console_config_command(expression")
    assert command < body.index("hs_compile_and_evaluate(expression)")
    assert "continue;" in body[command:body.index("hs_compile_and_evaluate(expression)")]


def test_guest_reads_the_development_flag_from_the_host():
    imports = (ROOT / "port/runtime/host_imports.list").read_text().splitlines()
    assert imports.count("host_development_hooks") == 1
    host = (ROOT / "port/ios/host/host_benchmark.m").read_text()
    hooks = host[host.index("int host_development_hooks(void)"):]
    assert hooks.index("#ifdef HALO_DEVELOPMENT_HOOKS") < hooks.index("return 1;") < hooks.index("#else")


def test_live_set_changes_the_value_in_force(tmp_path):
    if not shutil.which("xcrun") or not shutil.which("pkg-config"):
        pytest.skip("the probe needs xcrun clang and SDL3's pkg-config")
    sdl = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "sdl3"], text=True))
    probe = tmp_path / "config-live-probe"
    subprocess.run(["xcrun", "clang", "-O1", "-fsanitize=address", "-DHALO_ILP32=1", "-Iport/linux/src",
                    "-Iport/third_party/tomlc17", "port/ios/tests/config_live_probe.c", *sdl, "-o", str(probe)],
                   cwd=ROOT, check=True)
    result = subprocess.run([str(probe)], cwd=ROOT, check=True, capture_output=True, text=True)
    assert "config_live_probe: ok" in result.stdout


def test_knob_chord_uses_the_one_live_set():
    chord = (ROOT / "port/linux/src/xinput_sdl.c").read_text()
    renderer = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    assert "config_set_live(name, value, note, sizeof(note))" in chord
    assert "halo_knob_apply" not in chord and "int halo_knob_apply(" not in renderer
    assert "config_set_live_hook(knob_live_apply);" in renderer
