"""Two launches of one replay save the same moments under the same screenshot names.

Under debug.fixed_timestep every game tick repeats from launch to launch, but the frames presented before the game
starts (menus, loading, a network game's setup) do not: on one Mac the 64-player replay's first tick fell on frame 746
in one launch and on frame 863 in the next. Shots counted by presented frame then show different ticks under the same
name. debug.screenshot_clock = "ticks" (port/linux/src/screenshot_schedule.h) counts and names shots by tick. These
tests compile the schedule and drive it as port/linux/src/d3d8_device.c does, one call per presented frame.
"""

import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEDULE = ROOT / "port/linux/src/screenshot_schedule.h"

PROBE = r"""
#include <stdarg.h>
#include <stdio.h>
#include "screenshot_schedule.h"

static long current_tick = -1, asked;
static long shown_tick(void) { asked++; return current_tick; }
static void log_line(const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	printf("log ");
	vprintf(format, arguments);
	printf("\n");
	va_end(arguments);
}

/* `menu` frames outside a game, then ticks first..last, each shown by `frames_per_tick` frames; prints "name tick"
for every shot saved */
static void play(struct screenshot_schedule *schedule, unsigned long *frame, unsigned long menu, long first, long last,
	int frames_per_tick)
{
	unsigned long number, end = *frame + menu;
	long tick;
	int repeat;

	current_tick = -1;
	for (; *frame < end; ++*frame)
		if (screenshot_schedule_due(schedule, *frame, shown_tick, &number))
			printf("%s menu\n", schedule->name);
	for (tick = first; tick <= last; tick++)
		for (current_tick = tick, repeat = 0; repeat < frames_per_tick; repeat++, ++*frame)
			if (screenshot_schedule_due(schedule, *frame, shown_tick, &number))
				printf("%s %ld\n", schedule->name, tick);
}

static void launch(const char *clock, unsigned long menu, int frames_per_tick)
{
	struct screenshot_schedule schedule;
	unsigned long frame = 0;

	screenshot_schedule_init(&schedule, clock, 50, log_line);
	play(&schedule, &frame, menu, 0, 2312, frames_per_tick);
	printf("--\n");
}

int main(void)
{
	struct screenshot_schedule schedule;
	unsigned long frame = 0;

	launch("ticks", 746, 1);
	launch("ticks", 863, 1);
	launch("ticks", 863, 2);
	launch("frames", 746, 1);
	launch("frames", 863, 1);
	/* a checkpoint revert, then a new map after the menus */
	screenshot_schedule_init(&schedule, "ticks", 50, log_line);
	play(&schedule, &frame, 10, 0, 160, 1);
	play(&schedule, &frame, 0, 60, 160, 1);
	play(&schedule, &frame, 30, 0, 110, 1);
	printf("--\n");
	/* no shots, or the frame clock: the game's time is never asked for (before the game exists it halts) */
	asked = 0;
	screenshot_schedule_init(&schedule, "ticks", 0, log_line);
	play(&schedule, &frame, 5, 0, 5, 1);
	screenshot_schedule_init(&schedule, "frames", 50, log_line);
	play(&schedule, &frame, 5, 0, 5, 1);
	printf("asked %ld\n--\n", asked);
	/* an unknown clock is logged and counts frames */
	screenshot_schedule_init(&schedule, "tiks", 50, log_line);
	frame = 100;
	play(&schedule, &frame, 1, 0, 0, 1);
	printf("--\n");
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


def shots(lines):
    return [tuple(line.split()) for line in lines if not line.startswith("log ")]


def test_tick_clock_names_the_same_ticks_in_every_launch():
    ticks_746, ticks_863, ticks_slow = run_probe()[:3]
    # however many frames came before the game, and however many frames show each tick
    assert ticks_746 == ticks_863 == ticks_slow
    assert shots(ticks_746) == [(f"tick{tick:05d}", str(tick)) for tick in range(0, 2313, 50)]


def test_frame_clock_names_are_unchanged_and_show_different_ticks():
    frames_746, frames_863 = run_probe()[3:5]
    assert all(name == f"frame{int(name[5:]):05d}" for name, _ in shots(frames_746))
    # the class the tick clock replaces: one name, a different tick in each launch
    a = {name: tick for name, tick in shots(frames_746) if tick != "menu"}
    b = {name: tick for name, tick in shots(frames_863) if tick != "menu"}
    assert any(a[name] != b[name] for name in a.keys() & b.keys())


def test_a_tick_restart_never_reuses_a_name():
    restart = run_probe()[5]
    names = [name for name, _ in shots(restart)]
    assert len(names) == len(set(names))
    assert names == ["tick00000", "tick00050", "tick00100", "tick00150",
                     "tick00100-restart1", "tick00150-restart1",
                     "tick00000-restart2", "tick00050-restart2", "tick00100-restart2"]
    logs = [line for line in restart if line.startswith("log ")]
    assert ["went back from 160 to 60;" in logs[0], "went back from 160 to 0;" in logs[1]] == [True, True]


def test_the_game_time_is_read_only_for_tick_shots():
    assert run_probe()[6] == ["asked 0"]


def test_an_unknown_clock_is_logged_and_counts_frames():
    unknown = run_probe()[7]
    assert unknown[0].startswith('log debug.screenshot_clock "tiks" is not')
    assert shots(unknown) == [("frame00100", "menu")]
