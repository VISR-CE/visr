"""Traced object shadows give back their memory when tracing stops (display.traced_shadows). The class: memory made
for tracing that stays for the session once the setting, the graphics mode, stereo or a failure turns tracing off.
The front end's frame decision and the game's window begin are compiled out of the source with their types stubbed;
the backend's release path is checked against every resource its traced sections declare."""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import cc, source as probe_source  # noqa: E402

DEVICE = ROOT / "port/linux/src/d3d8_device.c"
CASTERS = ROOT / "port/linux/game/traced_shadow_casters.c"
METAL = ROOT / "port/ios/host/gpu_metal.m"

FRONT_END = r"""
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define HALO_ILP32 1
#define HALO_STEREO_OFF 0
#define CONFIG_SCENE_GAMEPLAY 0
#define TRACED_SHADOWS_RESET 1
#define TRACED_SHADOWS_NO_EPOCH 0xffffffffu
typedef uint32_t gpu_texture;

static struct { unsigned long frame; BOOL gl_ready; } device = { 0, TRUE };
static struct { BOOL msl; } shader_dialect = { TRUE };
struct halo_stereo_frame { int mode, eye_count; };
static const struct halo_stereo_frame *halo_stereo_frame(void) { static struct halo_stereo_frame off; return &off; }
static int setting = 1, ready = 1, destroyed, resets, reset_epoch_wrong;
static int config_scene_boolean(const char *key, int scene) { (void)key; (void)scene; return setting; }
int config_boolean(const char *key);
static uint32_t host_traced_shadows_ready(uint32_t any_gpu) { (void)any_gpu; return (uint32_t)ready; }
static void gpu_texture_destroy(gpu_texture texture) { (void)texture; destroyed++; }
static void host_traced_shadows_data(uint32_t kind, uint32_t id, const void *bytes, uint32_t size)
{
	(void)bytes; (void)size;
	if (kind == TRACED_SHADOWS_RESET) { resets++; reset_epoch_wrong += id != TRACED_SHADOWS_NO_EPOCH; }
}

static struct
{
	unsigned long frame;
	BOOL frame_on;
	gpu_texture term;
	unsigned long term_width, term_height;
	unsigned long releases;
} traced_shadows;

/*@release@*/
/*@releases@*/
/*@frame_on@*/

/* argv: one letter a frame: o on, s setting off, f the backend failed; each frame makes a term while on */
int main(int argc, char **argv)
{
	const char *frame;

	(void)argc;
	for (frame = argv[1]; *frame; frame++)
	{
		setting = *frame != 's';
		ready = *frame != 'f';
		device.frame++;
		if (traced_shadows_frame_on() && !traced_shadows.term)
		{
			traced_shadows.term = 7;
			traced_shadows.term_width = traced_shadows.term_height = 64;
		}
	}
	printf("%d %d %lu %d %d\n", destroyed, resets, halo_traced_shadows_releases(), traced_shadows.term != 0,
		reset_epoch_wrong);
	return 0;
}
"""

GAME = r"""
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef unsigned char boolean;
typedef float real;
#define TRUE 1
#define FALSE 0
#define PART_TABLE_SIZE 4
#define TRACED_SHADOWS_RESET 1
struct traced_part { int sent; };
static struct traced_part part_table[PART_TABLE_SIZE];
static struct
{
	struct traced_part *parts;
	unsigned long part_count, epoch;
	boolean reset_sent;
	unsigned long releases;
	boolean window, world;
	unsigned long long frame;
	unsigned long next_id;
	boolean world_sent;
} traced;
static struct { unsigned long long fps_accumulation_frame_index; } rasterizer_globals;
static unsigned long releases;
static int resets_sent;
static uint32_t last_epoch;
static int halo_traced_shadows_window_begin(float p11) { (void)p11; return 1; }
static unsigned long halo_traced_shadows_releases(void) { return releases; }
static void host_traced_shadows_data(uint32_t kind, uint32_t id, const void *bytes, uint32_t size)
{
	(void)bytes; (void)size;
	if (kind == TRACED_SHADOWS_RESET) { resets_sent++; last_epoch = id; }
}
int config_boolean(const char *key);
static void traced_left_out_log(void) {}
static void world_send(void) {}

/*@new_map@*/
/*@window_begin@*/

/* argv: one letter a frame: w a traced window, r tracing stopped and came back before this frame, m a new map */
int main(int argc, char **argv)
{
	const char *frame;

	(void)argc;
	traced.parts = part_table;
	traced_shadow_casters_new_map();
	for (frame = argv[1]; *frame; frame++)
	{
		rasterizer_globals.fps_accumulation_frame_index++;
		if (*frame == 'r')
			releases++;
		if (*frame == 'm')
			traced_shadow_casters_new_map();
		part_table[0].sent = 1;
		traced_shadow_casters_window_begin(1.0f);
	}
	printf("%d %u %d\n", resets_sent, last_epoch, part_table[0].sent);
	return 0;
}
"""

# what the traced sections of gpu_metal.m make once and keep: programs, pipeline states and depth-stencil states
KEPT = {"traced_composites", "traced_vertex_function", "traced_composite_function", "traced_bounds_kernel",
        "traced_radius_down", "traced_radius_reach", "traced_blur", "traced_level", "traced_everything"}


def compile_harness(tmp_path, name, harness, functions):
    for placeholder, function in functions.items():
        harness = harness.replace(placeholder, function)
    source = tmp_path / f"{name}.c"
    source.write_text(harness)
    executable = tmp_path / name
    cc.build([source], output=executable, std="c99", extra_warnings=True, relaxed=["unused-function"], port_stubs=True)
    return executable


def front_end(tmp_path):
    source = DEVICE.read_text()
    return compile_harness(tmp_path, "front-end", FRONT_END, {
        "/*@release@*/": probe_source.function_in(source, "traced_shadows_release"),
        "/*@releases@*/": probe_source.function_in(source, "halo_traced_shadows_releases"),
        "/*@frame_on@*/": probe_source.function_in(source, "traced_shadows_frame_on"),
    })


def run(executable, frames):
    return subprocess.run([str(executable), frames], capture_output=True, text=True, check=True).stdout.split()


def test_tracing_that_stops_gives_back_the_term_and_the_backends_memory(tmp_path):
    executable = front_end(tmp_path)
    destroyed, resets, releases, term, wrong_epoch = run(executable, "ooos")
    assert (destroyed, resets, releases, term, wrong_epoch) == ("1", "1", "1", "0", "0")


def test_a_backend_failure_gives_back_memory_like_the_setting(tmp_path):
    executable = front_end(tmp_path)
    assert run(executable, "oof")[:4] == ["1", "1", "1", "0"]


def test_only_the_step_from_on_to_off_releases(tmp_path):
    executable = front_end(tmp_path)
    assert run(executable, "ssoooo")[:3] == ["0", "0", "0"]
    assert run(executable, "ossso")[:4] == ["1", "1", "1", "1"]
    assert run(executable, "osos")[:3] == ["2", "2", "2"]


def game(tmp_path):
    source = CASTERS.read_text()
    return compile_harness(tmp_path, "game", GAME, {
        "/*@new_map@*/": probe_source.function_in(source, "traced_shadow_casters_new_map", which="traced.epoch++;"),
        "/*@window_begin@*/": probe_source.function_in(source, "traced_shadow_casters_window_begin",
                                                            which="traced_shadow_casters_new_map();"),
    })


def test_the_game_sends_everything_again_in_a_new_epoch_after_a_release(tmp_path):
    executable = game(tmp_path)
    assert run(executable, "ww") == ["1", "1", "1"]
    # tracing stopped and came back: a new epoch, and the parts the game had sent are forgotten so they go again
    assert run(executable, "wwr") == ["2", "2", "0"]
    resets, epoch, part_sent = run(executable, "wmw")
    assert (resets, epoch) == ("2", "2")


def test_every_resource_tracing_makes_is_given_back_by_the_one_release_path():
    text = METAL.read_text()
    sections = probe_source.region(METAL, "traced-sections")
    declared = set()
    # (a strong static inside a function is declared here too: traced_release cannot name it, so it fails, which is
    # the point, since it would outlive every release)
    for declaration in re.findall(r"^\s*static __strong [^;(]*;", sections, re.MULTILINE):
        names = re.sub(r"<[^<>]*(<[^<>]*>[^<>]*)*>", "", declaration.strip()[len("static __strong "):-1])
        for part in names.split(","):
            name = re.findall(r"\*?\s*(\w+)\s*(?:\[[^\]]*\])?\s*$", part.strip())
            assert name, part
            declared.add(name[0])
    release = probe_source.function_in(text, "traced_release")
    for name in sorted(declared - KEPT):
        assert re.search(rf"\b{name}(\[\w+\])?\s*=\s*nil;", release), f"{name} is not given back by traced_release"
    assert "free(traced.part_vertices[index]);" in release
    # the release is carried out even after a failure, before the backend asks whether tracing is ready
    data = probe_source.function_in(text, "gpu_metal_traced_shadows_data")
    assert data.index("traced_release(id)") < data.index("gpu_metal_traced_shadows_ready(")


def enclosing_block(text, at):
    """The innermost brace block around offset `at`."""
    depth = 0
    start = at
    while start > 0:
        start -= 1
        if text[start] == "}":
            depth += 1
        elif text[start] == "{":
            if depth == 0:
                break
            depth -= 1
    depth = 0
    end = at
    while end < len(text):
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            if depth == 0:
                break
            depth -= 1
        end += 1
    return text[start:end + 1]


def test_every_pending_build_that_is_dropped_forgets_its_slot():
    # a build or refit queued for the frame either reaches the encoder or is dropped; a dropped one must forget its
    # slot (traced_forget), or the cache keeps believing the structure holds a pose it never got
    text = METAL.read_text()
    release = probe_source.function_in(text, "traced_release")
    sections = probe_source.region(METAL, "traced-sections").replace(release, "")
    drops = list(re.finditer(r"traced_pending_descriptors\[([^\]]+)\]\s*=\s*nil;", sections))
    assert len(drops) >= 3, "the frame check, the scratch failure and the commit each drop pending descriptors"
    for drop in drops:
        index = re.escape(drop.group(1))
        block = enclosing_block(sections, drop.start())
        encoded = re.search(rf"(build|refit)AccelerationStructure:\w+ descriptor:traced_pending_descriptors\[{index}\]",
            block)
        forgotten = re.search(rf"traced_forget\(traced\.pending_slot\[{index}\]\)", block)
        assert encoded or forgotten, f"a pending build dropped here is neither encoded nor forgotten:\n{block}"
