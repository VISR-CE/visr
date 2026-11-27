"""The blend between ticks is timed by each frame's presentation, measured from the loop clock's read.

Bug class: a frame's display lead measured from the wrong instant (after the tick, after the
Compositor's wait) or its running average cleared every frame, so the world's motion between
two presentations no longer follows the time between them. The harness below compiles the
display-schedule section of `render_interpolation.c` with `interpolation_lead.c`, drives it
through synthetic schedules (the headset's uneven 2,1 cadence, a flat 60 Hz with slow ticks,
steady cadences, the space closing) and checks what a frame shows against when it is seen.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "port/linux/game"

PREAMBLE = r"""
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef float real;
#define TRUE 1
#define FALSE 0
#define TICKS_PER_SECOND 30
#define PIN(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
static double now_ms;
double halo_frame_trace_milliseconds(void) { return now_ms; }
int platform_fixed_timestep(void) { return 0; }
real game_time_get_speed(void) { return 1.0f; }
/* the last "stereo lead" line's pinned counts, and how many such lines */
static unsigned long logged_pinned[2], lead_lines;
void platform_log(const char *format, ...)
{
	char line[512];
	const char *pinned;
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(line, sizeof(line), format, arguments);
	va_end(arguments);
	if (strncmp(line, "interpolation: stereo lead", 26) || !(pinned = strstr(line, "pinned ")))
		return;
	if (sscanf(pinned, "pinned %lu with a tick, %lu without", &logged_pinned[1], &logged_pinned[0]) == 2)
		lead_lines++;
}
static int lead_setting = 1;
int config_boolean(const char *name) { (void)name; return lead_setting; }
static int loop_frame_ticked;
#include "interpolation_lead.h"
"""

DRIVER = r"""
#define TICK_MS (1000.0 / 30.0)
static int failures;
static int harness_repeat; /* the Compositor's repeat count the stereo frames open with */

static void check(int ok, const char *what, double value)
{
	if (!ok) {
		printf("FAIL %s (%.4f)\n", what, value);
		failures++;
	}
}

/* the game time a frame shows, in milliseconds: its clock read's tick plus the blended fraction */
static double shown(double clock_read, real fraction)
{
	double tick_start = floor(clock_read / TICK_MS) * TICK_MS;
	return tick_start + (double)fraction * TICK_MS;
}

/* one stereo loop frame: the clock read, the tick, game_time_update's call, the Compositor's wait,
   the frame's own call, and a flat-style reset at the present (which must not touch stereo) */
static double stereo_frame(double read, double tick_ms, double opened, double presented, double *early_shown)
{
	double tick_start = floor(read / TICK_MS) * TICK_MS;
	real fraction = (real)((read - tick_start) / TICK_MS);
	real early, drawn;

	now_ms = read;
	render_interpolation_clock_read();
	loop_frame_ticked = tick_ms > 0.0;
	now_ms = read + tick_ms;
	early = fraction_for_display(fraction);
	now_ms = opened;
	render_interpolation_presentation_due((unsigned long)((presented - opened) * 1000.0), harness_repeat);
	drawn = fraction_for_display(fraction);
	render_interpolation_next_frame_due(0);
	if (early_shown)
		*early_shown = shown(read, early);
	return shown(read, drawn);
}

/* the headset's 2,1 cadence (the cadence spec, section 3): a tick frame reads the clock 6 ms into the
   tick and is seen 46 ms later; the next reads at 28 ms and is seen 35 ms later */
static void uneven_stereo(void)
{
	double offset = 0.0, worst = 0.0, early_worst = 0.0;
	int n;

	for (n = 0; n < 600; n++) {
		double base = n * TICK_MS, early;
		double a = stereo_frame(base + 6.0, 10.0, base + 22.0, base + 52.0, &early);
		double a_offset = base + 52.0 - a, a_early = early;
		double b = stereo_frame(base + 28.0, 0.0, base + 33.0, base + 63.0, &early);
		double b_offset = base + 63.0 - b;

		if (n == 100)
			offset = a_offset;
		if (n > 100) {
			worst = fmax(worst, fmax(fabs(a_offset - offset), fabs(b_offset - offset)));
			early_worst = fmax(early_worst, fmax(fabs(a_early - a), fabs(early - b)));
		}
	}
	/* the shown time trails the presentation by one constant: equal motion per millisecond seen */
	check(worst < 0.5, "uneven stereo: shown time against presentation varies", worst);
	/* game_time_update's call (effects) lands with the frame's own */
	check(early_worst < 0.5, "uneven stereo: the early call's estimate strays from the frame", early_worst);
}

/* steady cadences: every lead the same, so no correction and the clock's fraction is drawn */
static void steady_stereo(double period)
{
	int n;
	double worst = 0.0;

	render_interpolation_presentation_due(0, 0);
	for (n = 0; n < 300; n++) {
		double read = 3.0 + n * period;
		double tick_start = floor(read / TICK_MS) * TICK_MS;
		real fraction = (real)((read - tick_start) / TICK_MS);
		double drawn = stereo_frame(read, n % 3 == 0 ? 9.0 : 0.0, read + 12.0, read + 40.0, NULL);

		if (n > 20)
			worst = fmax(worst, fabs(drawn - shown(read, fraction)));
	}
	check(worst < 0.05, "steady stereo: a constant lead changed the blend", worst);
}

/* flat 60 Hz: each present says when the next frame shows, 30 ms after its clock read; tick
   frames spend 12 ms in the tick before either call, which must not shorten their lead */
static void flat_slow_ticks(int eyes_fell_through)
{
	int n;
	double worst = 0.0;

	render_interpolation_presentation_due(0, 0);
	render_interpolation_next_frame_due(0);
	for (n = 0; n < 400; n++) {
		/* a two-eye frame without the Compositor (side by side, or the headset not ready) reports no
		presentation when it begins and is shown, and scheduled, by the window's present */
		if (eyes_fell_through)
			render_interpolation_presentation_due(0, 0);
		double read = 1.0 + n * (1000.0 / 60.0);
		double tick_start = floor(read / TICK_MS) * TICK_MS;
		int ticked = floor((read - 1000.0 / 60.0) / TICK_MS) != floor(read / TICK_MS);
		real fraction = (real)((read - tick_start) / TICK_MS);
		real early, drawn;

		now_ms = read;
		render_interpolation_clock_read();
		loop_frame_ticked = ticked;
		now_ms = read + (ticked ? 12.0 : 0.5);
		early = fraction_for_display(fraction);
		now_ms += 4.0;
		drawn = fraction_for_display(fraction);
		if (n > 20) {
			worst = fmax(worst, fabs(shown(read, drawn) - shown(read, fraction)));
			worst = fmax(worst, fabs(shown(read, early) - shown(read, fraction)));
		}
		/* the present: the next frame shows 30 ms after its own clock read */
		now_ms = read + 14.0;
		render_interpolation_next_frame_due((unsigned long)((read + 1000.0 / 60.0 + 30.0 - now_ms) * 1000.0));
	}
	check(worst < 0.05, eyes_fell_through ? "two eyes in the window: the window's schedule was dropped"
		: "flat: a tick frame's lead read short", worst);
}

/* the space closing: no Compositor frame, no stereo correction left behind */
static void space_closes(void)
{
	double read = 50000.0;
	real fraction;

	stereo_frame(read, 10.0, read + 16.0, read + 52.0, NULL);
	stereo_frame(read + 22.0, 0.0, read + 27.0, read + 57.0, NULL);
	read += 44.0;
	now_ms = read;
	render_interpolation_clock_read();
	loop_frame_ticked = 0;
	render_interpolation_presentation_due(0, 0);
	fraction = fraction_for_display(0.4f);
	check(fraction == 0.4f, "space closed: a stereo correction outlived the space", fraction);
}

/* stereo frames that stop without reporting 0 (a path that forgot): the schedule lapses after one frame */
static void stereo_stops_silently(void)
{
	double read = 90000.0;
	real fraction;
	int n;

	render_interpolation_presentation_due(0, 0);
	for (n = 0; n < 40; n++)
		stereo_frame(read + n * 22.2, n % 2 ? 0.0 : 10.0, read + n * 22.2 + 16.0, read + n * 22.2 + (n % 2 ? 35.0 : 46.0), NULL);
	for (n = 0; n < 3; n++) {
		now_ms = read + 900.0 + n * 16.7;
		render_interpolation_clock_read();
		loop_frame_ticked = n == 0;
		fraction = fraction_for_display(0.4f);
	}
	check(fraction == 0.4f, "stereo stopped without a report: its correction outlived it", fraction);
}

/* display.presentation_lead off: the headset's picture as before (no correction), for the session's comparison */
static void setting_off(void)
{
	double worst = 0.0;
	int n;

	halo_presentation_lead_toggle();
	render_interpolation_presentation_due(0, 0);
	for (n = 0; n < 200; n++) {
		double base = 70000.0 + n * TICK_MS;
		double read = base + (n % 2 ? 28.0 : 6.0);
		double tick_start = floor(read / TICK_MS) * TICK_MS;
		real fraction = (real)((read - tick_start) / TICK_MS);
		double drawn = stereo_frame(read, n % 2 ? 0.0 : 10.0, read + 16.0, read + (n % 2 ? 35.0 : 46.0), NULL);

		worst = fmax(worst, fabs(drawn - shown(read, fraction)));
	}
	check(worst < 0.001, "setting off: the headset was corrected", worst);
	halo_presentation_lead_toggle();
}

/* a repeat-count change (the knob's 0 to 1) moves every lead by about 25 ms: the average starts again from the
new count's leads, so no frame is corrected by the jump or pinned at 1 while the average catches up, also when the
Compositor's predicted presentation follows the count LAG frames late (the count reads back first). With the count
unchanged (the control) the same jump does pin frames, which the stereo lead line counts */
static unsigned long repeat_change(int reseeded, int lag)
{
	double worst = 0.0, base = 120000.0 + (reseeded ? 0.0 : 80000.0) + lag * 200000.0;
	unsigned long most_pinned = 0, lines;
	int n;

	render_interpolation_presentation_due(0, 0);
	harness_repeat = 0;
	/* (600 frames: a stereo lead line ends inside them, so the lines after the change count no earlier test's) */
	for (n = 0; n < 600; n++) {
		double read = base + 3.0 + n * (1000.0 / 90.0);
		stereo_frame(read, n % 3 == 0 ? 9.0 : 0.0, read + 12.0, read + 36.0, NULL);
	}
	harness_repeat = reseeded ? 1 : 0;
	base += 600 * (1000.0 / 90.0);
	lines = lead_lines;
	for (n = 0; n < 1300; n++) {
		double read = base + 3.0 + n * (1000.0 / 45.0);
		double tick_start = floor(read / TICK_MS) * TICK_MS;
		real fraction = (real)((read - tick_start) / TICK_MS);
		double lead = n < lag ? 36.0 : 61.0;
		double drawn = stereo_frame(read, n % 3 == 0 ? 9.0 : 0.0, read + 12.0, read + lead, NULL);

		worst = fmax(worst, fabs(drawn - shown(read, fraction)));
		if (lead_lines != lines) {
			lines = lead_lines;
			most_pinned = fmax(most_pinned, logged_pinned[0] + logged_pinned[1]);
		}
	}
	harness_repeat = 0;
	check(lead_lines > 0, "repeat change: no stereo lead line with a pinned count", 0.0);
	if (reseeded) {
		check(worst < 0.05, "repeat change: the old count's average corrected the new count's frames", worst);
		check(most_pinned == 0, "repeat change: frames pinned after the re-seed", (double)most_pinned);
	}
	return most_pinned;
}

int main(void)
{
	uneven_stereo();
	steady_stereo(1000.0 / 90.0);
	steady_stereo(1000.0 / 45.0);
	flat_slow_ticks(0);
	uneven_stereo();
	flat_slow_ticks(1);
	space_closes();
	stereo_stops_silently();
	setting_off();
	repeat_change(1, 0);
	repeat_change(1, 1);
	repeat_change(1, 2);
	check(repeat_change(0, 0) > 0, "repeat change control: a 25 ms jump in the lead pinned no frames", 0.0);
	return failures ? 1 : 0;
}
"""


def schedule_section() -> str:
    source = (GAME / "render_interpolation.c").read_text()
    start = source.index("/* ---------- the display's schedule")
    end = source.index("/* d3d8_device.c: debug.render_scale_dpad")
    return source[start:end]


def test_interpolation_lead_follows_presentation(tmp_path):
    harness = tmp_path / "interpolation_lead_harness.c"
    harness.write_text(PREAMBLE + schedule_section() + DRIVER)
    executable = tmp_path / "interpolation-lead"
    subprocess.run([
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(GAME),
        str(harness), str(GAME / "interpolation_lead.c"), "-lm", "-o", str(executable),
    ], check=True)
    result = subprocess.run([str(executable)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr


def test_no_present_reads_the_cpu_split_twice():
    """`host_frame_cpu_split` hands over the frame's split and clears it, so a second read in the same function gets
    nothing: every reader (the stereo frame's measurement, the flat present, the dispatch) reads it once, and the
    stereo frame's cadence line takes its tick times from the read the benchmark frame also uses."""
    for path in ("port/ios/host/gpu_metal.m", "port/ios/host/host_gpu_dispatch.c"):
        source = (ROOT / path).read_text()
        for function in source.split("\n}\n"):
            assert function.count("host_frame_cpu_split(") <= 1, f"{path}: a function reads the CPU split twice"
    source = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    start = source.index("static void stereo_pacing_count(int view, id<MTLTexture> picture)\n{")
    body = source[start:source.index("\n}\n", start)]
    assert body.count("host_frame_cpu_split(") == 1
    assert "reading.simulation > 0.0 ? reading.simulation" in body


def test_every_window_present_schedules_the_next_frame():
    """A present that can show in the window hands its schedule on: `gpu_present_stereo` falls through to the
    window's present when the Compositor is not there, and the schedule ignores it while stereo leads."""
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    presents = re.findall(r"^[^\n/]*\bgpu_present(?:_stereo)?\([^\n]*$", source, re.M)
    assert presents, "no presents found in d3d8_device.c"
    for line in presents:
        assert "render_interpolation_next_frame_due(" in line, f"a present's schedule is dropped: {line.strip()}"
