"""debug.benchmark_own_input reaches the game: both input stages that act on a benchmark game's controller 1 (the
scripted override in xinput_sdl.c and the suite's replay filter in input_replay.c) take the production functions,
so a stage that still zeroes or replaces the player's pad fails here (the bug class: one stage honored the setting
and the next undid it)."""
from pathlib import Path
import re
import shutil
import subprocess

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]

FIXTURE = r'''
#include <assert.h>
#include <string.h>
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef struct { WORD wButtons; BYTE bAnalogButtons[8]; short sThumbLX,sThumbLY,sThumbRX,sThumbRY; } XINPUT_GAMEPAD;
enum { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
       XINPUT_GAMEPAD_BLACK, XINPUT_GAMEPAD_WHITE, XINPUT_GAMEPAD_LEFT_TRIGGER, XINPUT_GAMEPAD_RIGHT_TRIGGER };
#define XINPUT_GAMEPAD_BACK 32
#define XINPUT_GAMEPAD_START 16
enum mp_benchmark_probe_input_policy { mp_benchmark_probe_ordinary_input, mp_benchmark_probe_neutral_input,
	mp_benchmark_probe_scripted_input };
static enum mp_benchmark_probe_input_policy policy;
static int own_input, paused;
static enum mp_benchmark_probe_input_policy mp_benchmark_probe_current_policy(void) { return policy; }
static int mp_benchmark_probe_own_input(void) { return own_input; }
static void mp_benchmark_probe_gamepad(XINPUT_GAMEPAD *pad, short port) { memset(pad, 0, sizeof(*pad)); pad->sThumbLX = 1234 + port; }
static void test_input_gamepad(XINPUT_GAMEPAD *pad) { (void)pad; }
/* input_replay.c's state, as a suite game with no recording has it */
static struct { int configured, suite, replay_finished, record_ended; char record_path[8], replay_path[8]; } replay = { 1, 1 };
static struct { int automatic; } take;
static void configure(void) {}
static void take_update(void) {}
static double game_ticks(void) { return 100.0; }
static void record_end(const char *why) { (void)why; }
static int game_time_get_paused(void) { return paused; }
static void record(XINPUT_GAMEPAD *pad, double ticks) { (void)pad; (void)ticks; }
static void play(XINPUT_GAMEPAD *pad, double ticks) { (void)pad; (void)ticks; }
static int config_boolean(const char *name) { (void)name; return 0; }
'''

MAIN = r'''
/* xinput_sdl.c's order for controller 1 (checked against the source by test_pipeline_order_matches_the_source) */
static XINPUT_GAMEPAD poll(short port)
{
	XINPUT_GAMEPAD pad = {0};
	pad.sThumbRX = 20000;
	pad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = 255;
	if (mp_benchmark_probe_or_test_gamepad(&pad, port) == mp_benchmark_probe_ordinary_input)
		input_replay_filter(&pad);
	return pad;
}
int main(void)
{
	XINPUT_GAMEPAD pad;
	/* a running benchmark game with the setting: the player's own pad reaches the game */
	policy = mp_benchmark_probe_scripted_input; own_input = 1;
	pad = poll(0);
	assert(pad.sThumbRX == 20000 && pad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] == 255);
	/* the other controllers keep their scripts */
	pad = poll(1);
	assert(pad.sThumbLX == 1235 && pad.sThumbRX == 0);
	/* without the setting: the script plays controller 1 */
	own_input = 0;
	pad = poll(0);
	assert(pad.sThumbLX == 1234 && pad.sThumbRX == 0);
	/* before the game runs (neutral): nobody plays, whatever the setting */
	policy = mp_benchmark_probe_neutral_input;
	pad = poll(0);
	assert(pad.sThumbRX == 0 && pad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] == 0);
	/* a suite scene with no recording and no setting stands still */
	policy = mp_benchmark_probe_ordinary_input;
	pad = poll(0);
	assert(pad.sThumbRX == 0);
	/* outside a suite the filter leaves ordinary play alone */
	replay.suite = 0;
	pad = poll(0);
	assert(pad.sThumbRX == 20000);
	return 0;
}
'''


def test_own_input_reaches_the_game(tmp_path):
    compiler = shutil.which("clang") or shutil.which("cc")
    assert compiler, "a C compiler is required"
    xinput = (ROOT / "port/linux/src/xinput_sdl.c").read_text()
    replay = (ROOT / "port/linux/src/input_replay.c").read_text()
    source = (FIXTURE + probe_source.function_in(xinput, "mp_benchmark_probe_or_test_gamepad") +
              "\n" + probe_source.function_in(replay, "input_replay_filter") + "\n" + MAIN)
    source_file = tmp_path / "benchmark_own_input.c"
    binary = tmp_path / "benchmark_own_input"
    source_file.write_text(source)
    built = subprocess.run([compiler, "-std=gnu11", "-O1", "-fsanitize=address,undefined", str(source_file), "-o", str(binary)],
                           capture_output=True, text=True)
    assert built.returncode == 0, built.stderr
    ran = subprocess.run([str(binary)], capture_output=True, text=True)
    assert ran.returncode == 0, ran.stderr


def test_pipeline_order_matches_the_source():
    """the fixture's poll() is xinput_sdl.c's order: the override, then the replay filter only for ordinary input"""
    xinput = (ROOT / "port/linux/src/xinput_sdl.c").read_text()
    override = xinput.index("probe_policy = mp_benchmark_probe_or_test_gamepad(&state->Gamepad, port);")
    filtered = re.search(r"if \(probe_policy == mp_benchmark_probe_ordinary_input\)\s+input_replay_filter\(&state->Gamepad\);", xinput)
    assert filtered and filtered.start() > override
