"""The level's structure traced shadows hold is the loaded bsp's (debug.traced_shadows_world,
port/linux/game/traced_shadow_casters.c). The class: data kept from a bsp keyed on addresses, which repeat because the
game loads each bsp at the same place, so a switch keeps the last level's triangles blocking rays. The key is the bsp's
index and its load (scenario.c's global_structure_bsp_loads); the decision is compiled out of the source."""
from pathlib import Path
import subprocess

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]
CASTERS = ROOT / "port/linux/game/traced_shadow_casters.c"
SCENARIO = ROOT / "source/scenario/scenario.c"

HARNESS = r"""
#include <stdio.h>

typedef unsigned char boolean;
#define TRUE 1
#define FALSE 0
static short global_structure_bsp_index = -1;
static unsigned long global_structure_bsp_loads;
static struct { boolean world_sent; short world_bsp_index; unsigned long world_bsp_loads; } traced;

/*@current@*/

/* argv: one letter a step: n a new map, l the bsp of index 0 loaded again, s a switch to index 1, w the world sent;
prints whether what was sent is current after each step */
int main(int argc, char **argv)
{
	const char *step;

	(void)argc;
	for (step = argv[1]; *step; step++)
	{
		if (*step == 'n')
			traced.world_sent = FALSE;
		if (*step == 'l' || *step == 's')
		{
			global_structure_bsp_index = *step == 's';
			global_structure_bsp_loads++;
		}
		if (*step == 'w' && !world_current())
		{
			traced.world_sent = TRUE;
			traced.world_bsp_index = global_structure_bsp_index;
			traced.world_bsp_loads = global_structure_bsp_loads;
		}
		printf("%d", world_current() ? 1 : 0);
	}
	printf("\n");
	return 0;
}
"""


def build(tmp_path):
    current = probe_source.function_in(CASTERS.read_text(), "world_current")
    source = tmp_path / "world-key.c"
    source.write_text(HARNESS.replace("/*@current@*/", current))
    executable = tmp_path / "world-key"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(executable)], check=True)
    return executable


def run(executable, steps):
    return subprocess.run([str(executable), steps], capture_output=True, text=True, check=True).stdout.strip()


def test_a_bsp_loaded_again_at_the_same_place_is_another_level(tmp_path):
    executable = build(tmp_path)
    assert run(executable, "lwlw") == "0101"
    assert run(executable, "lwsw") == "0101"
    assert run(executable, "lwnw") == "0101"


def test_the_level_is_keyed_on_the_bsp_not_its_addresses():
    casters = CASTERS.read_text()
    world_send = probe_source.function_in(casters, "world_send")
    assert "world_current()" in world_send
    assert "hardware_format" not in probe_source.function_in(casters, "world_current")
    switch = probe_source.function_in(SCENARIO.read_text(), "scenario_switch_structure_bsp")
    loaded = switch[switch.index("if (loaded)\n"):]
    assert "global_structure_bsp_loads++;" in loaded
