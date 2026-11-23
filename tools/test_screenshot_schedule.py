"""Two launches of one replay save the same moments under the same screenshot names.

Under debug.fixed_timestep every game tick repeats from launch to launch, but the frames presented before the game
starts (menus, loading, a network game's setup) do not: on one Mac the 64-player replay's first tick fell on frame 746
in one launch and on frame 863 in the next. Shots counted by presented frame then show different ticks under the same
name. debug.screenshot_clock = "ticks" (port/linux/src/screenshot_schedule.h) counts and names shots by tick.
"""

import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEDULE = ROOT / "port/linux/src/screenshot_schedule.h"
DEVICE = ROOT / "port/linux/src/d3d8_device.c"

PROBE = r"""
#include <stdio.h>
#include "screenshot_schedule.h"

static long current_tick = -1, asked;
static long shown_tick(void) { asked++; return current_tick; }

/* a launch: `menu` frames outside a game, then `ticks` game ticks, each shown by `frames_per_tick` frames;
prints "name tick" for every shot it saves */
static void launch(enum screenshot_clock clock, unsigned long menu, long ticks, int frames_per_tick)
{
	struct screenshot_schedule schedule;
	unsigned long frame = 0, number;
	long tick;
	int repeat;

	screenshot_schedule_init(&schedule, clock, 50);
	current_tick = -1;
	for (; frame < menu; frame++)
		if (screenshot_schedule_due(&schedule, frame, shown_tick, &number))
			printf("%lu menu\n", number);
	for (tick = 0; tick < ticks; tick++)
		for (current_tick = tick, repeat = 0; repeat < frames_per_tick; repeat++, frame++)
			if (screenshot_schedule_due(&schedule, frame, shown_tick, &number))
				printf("%lu %ld\n", number, tick);
	printf("--\n");
}

int main(void)
{
	launch(SCREENSHOT_CLOCK_TICKS, 746, 2313, 1);
	launch(SCREENSHOT_CLOCK_TICKS, 863, 2313, 1);
	launch(SCREENSHOT_CLOCK_TICKS, 863, 2313, 2);
	launch(SCREENSHOT_CLOCK_FRAMES, 746, 2313, 1);
	launch(SCREENSHOT_CLOCK_FRAMES, 863, 2313, 1);
	/* no shots, or the frame clock: the game's time is never asked for (before the game exists it halts) */
	{
		struct screenshot_schedule off;
		unsigned long number;
		asked = 0;
		screenshot_schedule_init(&off, SCREENSHOT_CLOCK_TICKS, 0);
		screenshot_schedule_due(&off, 0, shown_tick, &number);
		screenshot_schedule_init(&off, SCREENSHOT_CLOCK_FRAMES, 50);
		screenshot_schedule_due(&off, 0, shown_tick, &number);
		printf("asked %ld\n--\n", asked);
	}
	return 0;
}
"""


def run_probe():
    with tempfile.TemporaryDirectory() as tmp:
        source, binary = Path(tmp) / "probe.c", Path(tmp) / "probe"
        source.write_text(PROBE)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", f"-I{SCHEDULE.parent}", str(source), "-o", str(binary)],
                       check=True)
        output = subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout
    return [block.strip().splitlines() for block in output.split("--")[:-1]]


def test_tick_clock_names_the_same_ticks_in_every_launch():
    ticks_746, ticks_863, ticks_slow, frames_746, frames_863, asked = run_probe()
    assert asked == ["asked 0"]  # no shots, or the frame clock: the game's time is never read
    assert ticks_746 == ticks_863 == ticks_slow  # however many frames came first, and however many show each tick
    assert len(ticks_746) == 2313 // 50 + 1
    assert all(name == tick for name, tick in (line.split() for line in ticks_746))
    # the frame clock, the class this replaces: the same names show different ticks
    shown = lambda lines: {name: tick for name, tick in (line.split() for line in lines) if tick != "menu"}
    a, b = shown(frames_746), shown(frames_863)
    assert any(a[name] != b[name] for name in a.keys() & b.keys())


def test_the_device_schedules_shots_only_through_the_schedule():
    text = DEVICE.read_text(encoding="latin-1")
    # the tick is passed as a function, so a frame with no shot due never reads the game's time
    assert "render_interpolation_shown_tick()" not in text
    assert "screenshot_schedule_due(" in text
    assert not re.search(r"device\.frame\s*%\s*\(unsigned long\)\s*screenshot_every", text)
    assert text.count('"%s/frame%05lu%s.png"') == 1  # only screenshot_path names a shot by frame
