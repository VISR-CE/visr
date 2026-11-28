"""Shadows across the windows of one frame when some trace them and some draw maps (display.traced_shadows,
source/rasterizer/xbox/rasterizer_xbox_shadows.c). The class: a shadow generated on one window's path applied by a
window on the other, which samples a map tile nothing drew; and a window's decision taken from its first shadow
alone. The two production functions that decide are compiled out of the source with the game's types stubbed."""
from pathlib import Path
import subprocess

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source/rasterizer/xbox/rasterizer_xbox_shadows.c"

HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>

typedef unsigned char boolean;
#define TRUE 1
#define FALSE 0
#define TRACED_SHADOW_NONE 0xfffffffful

struct deferred_shadow { unsigned long traced_id; };
static struct deferred_shadow deferred_shadows[8];
static short deferred_shadow_count, deferred_shadow_window_first;
static boolean window_traces;
static boolean traced_shadow_casters_window_traces(void) { return window_traces; }

/*@usable@*/
/*@traced@*/

/* argv: whether this window traces, the window's first entry, then each entry's traced id (n for none) */
int main(int argc, char **argv)
{
	int index;

	window_traces = (boolean)atoi(argv[1]);
	deferred_shadow_window_first = (short)atoi(argv[2]);
	for (index = 3; index < argc; index++)
		deferred_shadows[index - 3].traced_id = argv[index][0] == 'n' ? TRACED_SHADOW_NONE : strtoul(argv[index], NULL, 10);
	deferred_shadow_count = (short)(argc - 3);
	for (index = 0; index < deferred_shadow_count; index++)
		printf("%d", deferred_shadow_usable_here(&deferred_shadows[index]) ? 1 : 0);
	printf(" %d\n", deferred_shadows_traced() ? 1 : 0);
	return 0;
}
"""


def build(tmp_path):
    source = SOURCE.read_text()
    usable = probe_source.function_in(source, "deferred_shadow_usable_here")
    traced = probe_source.function_in(source, "deferred_shadows_traced")
    harness = tmp_path / "harness.c"
    harness.write_text(HARNESS.replace("/*@usable@*/", usable).replace("/*@traced@*/", traced))
    executable = tmp_path / "shadow-window-paths"
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", str(harness), "-o", str(executable)], check=True)
    return executable


def run(executable, traces, first, *entries):
    output = subprocess.run([str(executable), str(int(traces)), str(first), *entries], capture_output=True, text=True,
                            check=True).stdout.split()
    return output[0] if entries else "", output[-1] == "1"


def test_a_traced_shadow_is_applied_only_where_the_window_traces(tmp_path):
    executable = build(tmp_path)
    # a shadow traced in an earlier window, then a map shadow: a window without tracing may reuse only the map's
    assert run(executable, False, 0, "0", "n")[0] == "01"
    assert run(executable, True, 0, "0", "n")[0] == "11"


def test_a_window_counts_as_traced_only_when_all_its_new_shadows_are(tmp_path):
    executable = build(tmp_path)
    assert run(executable, True, 0, "0", "1")[1]
    # past the traced shadows' limit a window that traces still draws one into its map, which needs its blur
    assert not run(executable, True, 0, "0", "n")[1]
    assert not run(executable, True, 0, "n", "0")[1]
    # only this window's shadows count, not an earlier window's
    assert run(executable, True, 1, "n", "0", "1")[1]
    assert not run(executable, True, 1)[1]
