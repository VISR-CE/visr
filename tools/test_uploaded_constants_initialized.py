"""A vertex constant block built on the stack must not upload unwritten bytes.

Bug class: a block of shader constants assembled in a local struct and uploaded
whole carries whatever the stack held in the members nothing writes (padding
lanes, unused lights). Those bytes differ between two runs of one build, so the
GPU commands and `debug.gpu_call_hash` differ too. The model lighting block's
pads (c[-73].w to c[-69].w) were one instance.

The test compiles the production functions that build these blocks, fills the
stack with a pattern before each call, and checks that no uploaded word still
holds the pattern, across every light count and the debug ambient path.
"""
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from probekit import source as probe_source  # noqa: E402

RASTERIZER = ROOT / "source/rasterizer/xbox/rasterizer_xbox.c"
RENDER_H = ROOT / "source/render/render.h"
OBJECT_LIGHTS_H = ROOT / "source/objects/object_lights.h"
# the production limits the probe's arrays and loops are sized by
LIMIT_SOURCES = {
    "MAXIMUM_RENDERED_DISTANT_LIGHTS": RENDER_H,
    "MAXIMUM_RENDERED_POINT_LIGHTS": RENDER_H,
    "LIGHTS_PORT_MAXIMUM_POINT_LIGHTS": OBJECT_LIGHTS_H,
    "NUMBER_OF_MODEL_LIGHTING_CONSTANTS": RASTERIZER,
}

# Every function in rasterizer_xbox.c that uploads a vertex constant block from a
# local struct (`&local` as the data argument). A new one must be added to the
# probe. Blocks built in local arrays and passed by name (fog, screen and the
# environment shaders' constants) are outside this check.
COVERED_UPLOADERS = {"rasterizer_set_model_lighting", "rasterizer_port_extra_point_lights"}


def stack_struct_uploaders(text):
    """Functions that pass the address of a local to D3DDevice_SetVertexShaderConstant."""
    found = set()
    for call in re.finditer(r"D3DDevice_SetVertexShaderConstant\(\s*[^,]+,\s*&(\w+)", text):
        head = text[:call.start()]
        definition = list(re.finditer(r"(?m)^(?:static )?[A-Za-z_][^;\n(]*\b(\w+)\(", head))[-1]
        body = text[definition.start():call.start()]
        # a named struct local, or an anonymous one (`} name;`)
        if re.search(rf"(?m)^\s*(?:struct \w+ |\}}\s*){call.group(1)};", body):
            found.add(definition.group(1))
    return found


PROBE = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef float real;
typedef int16_t word;
typedef int32_t boolean;
#define long int32_t
#define NONE (-1)
#define TRUE 1
#define FALSE 0
typedef struct { real x, y, z; } real_point3d;
typedef struct { real i, j, k; } real_vector3d;
typedef struct { real red, green, blue; } real_rgb_color;
typedef struct { real alpha, red, green, blue; } real_argb_color;
LIMITS
#define match_assert(file, line, expr) do { if (!(expr)) { puts("assert: " #expr); _exit_probe(); } } while (0)
#define match_vassert(file, line, expr, message) match_assert(file, line, expr)
#define csprintf(buffer, ...) ""
#define csmemset memset
static char temporary[1];
static void _exit_probe(void) { fflush(stdout); __builtin_trap(); }
STRUCTS
struct point_light_definition { struct point_light_geometry_parameters geometry; };
struct rasterizer_light_submit_parameters { struct point_light_definition *definition;
	real_point3d position; real_vector3d forward; real_vector3d up; real_rgb_color color; real radius; };
static struct { long light_count; struct rasterizer_light_submit_parameters lights[8]; } rasterizer_lights;
static struct { real model_lighting_ambient; short statistics_mode; } rasterizer_debug_options;
static struct { long vertex_shader_lighting_constant_bytes; } rasterizer_frame_statistics;

#define PATTERN 0xA5
static int failures;
static const char *scene;
static void D3DDevice_SetVertexShaderConstant(int reg, const void *data, unsigned count)
{
	const uint32_t *words = data;
	for (unsigned i = 0; i < count * 4; i++)
		if (words[i] == 0xA5A5A5A5u) {
			printf("%s: c[%d].%c holds unwritten stack bytes\n", scene, reg + (int)(i / 4), "xyzw"[i % 4]);
			failures++;
		}
}
static long point_light_object_count;
static long config_integer(const char *name) { (void)name; return 8; }
static boolean config_boolean(const char *name) { (void)name; return TRUE; }
static short lights_port_point_lights_for_object(long object, long *indices, short limit)
{
	(void)object;
	short n = (short)(point_light_object_count < limit ? point_light_object_count : limit);
	for (short i = 0; i < n; i++) indices[i] = i;
	return n;
}
PRODUCTION

__attribute__((noinline)) static void dirty_stack(void)
{
	volatile unsigned char junk[16384];
	for (unsigned i = 0; i < sizeof(junk); i++) junk[i] = PATTERN;
}

int main(void)
{
	static struct point_light_definition definition;
	definition.geometry.runtime_cosine_falloff_angle = 0.9f;
	definition.geometry.runtime_cosine_cutoff_angle = 0.5f;
	rasterizer_lights.light_count = 8;
	for (int i = 0; i < 8; i++) {
		rasterizer_lights.lights[i].definition = &definition;
		rasterizer_lights.lights[i].radius = 1.0f + (real)i;
	}
	char name[96];
	for (int ambient = 0; ambient < 2; ambient++)
	for (int points = 0; points <= 2; points++)
	for (int distant = 0; distant <= 2; distant++)
	for (int object = 0; object < 2; object++) {
		struct render_lighting lighting;
		memset(&lighting, 0, sizeof(lighting));
		lighting.point_light_count = (short)points;
		lighting.distant_light_count = (short)distant;
		for (int i = 0; i < points; i++) lighting.point_light_indices[i] = i;
		rasterizer_debug_options.model_lighting_ambient = ambient ? 0.5f : 0.0f;
		rasterizer_port_point_light_object = object ? 1 : NONE;
		point_light_object_count = object ? 6 : 0;
		snprintf(name, sizeof(name), "ambient %d, %d point, %d distant, object %d", ambient, points, distant, object);
		scene = name;
		dirty_stack();
		rasterizer_set_model_lighting(&lighting);
	}
	return failures != 0;
}
"""


def limits():
    values = []
    for name, path in LIMIT_SOURCES.items():
        match = re.search(rf"\b{name}\s*=\s*(\d+)", path.read_text(encoding="latin-1"))
        assert match, f"Production constant missing: {name}"
        values.append(f"{name} = {match.group(1)}")
    return "enum { " + ", ".join(values) + " };\n"


def probe_program():
    rasterizer = RASTERIZER.read_text(encoding="latin-1")
    render = RENDER_H.read_text(encoding="latin-1")
    structs = "".join([
        probe_source.structure_in(render, "render_distant_light") + "\n",
        probe_source.structure_in(render, "render_lighting") + "\n",
        probe_source.structure_in(rasterizer, "rasterizer_point_light_constants") + "\n",
        probe_source.structure_in(rasterizer, "rasterizer_distant_light_constants") + "\n",
        probe_source.structure_in(rasterizer, "rasterizer_model_lighting_constants") + "\n",
        probe_source.structure_in(rasterizer, "point_light_geometry_parameters") + "\n",
    ])
    enum = re.search(r"(?m)^enum\s*\n\{\s*\n\s*PORT_EXTRA_POINT_LIGHT_CONSTANT[^}]*\};", rasterizer)
    assert enum, "Production enum missing: PORT_EXTRA_POINT_LIGHT_CONSTANT"
    production = "".join([
        "static long rasterizer_port_point_light_object = NONE;\n",
        enum.group(0), "\n",
        probe_source.function_in(rasterizer, "rasterizer_set_model_lighting_point_light") + "\n",
        probe_source.function_in(rasterizer, "rasterizer_set_model_lighting_distant_light") + "\n",
        probe_source.function_in(rasterizer, "rasterizer_port_extra_point_lights") + "\n",
        probe_source.function_in(rasterizer, "rasterizer_set_model_lighting") + "\n",
    ])
    return PROBE.replace("LIMITS", limits()).replace("STRUCTS", structs).replace("PRODUCTION", production)


def test_every_stack_built_constant_upload_is_probed():
    assert stack_struct_uploaders(RASTERIZER.read_text(encoding="latin-1")) == COVERED_UPLOADERS


@pytest.mark.parametrize("optimization", ["-O0", "-O2"])
def test_model_lighting_upload_has_no_unwritten_bytes(optimization):
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        pytest.skip("A C compiler is required")
    with tempfile.TemporaryDirectory(prefix="uploaded-constants-probe-") as temp:
        source = Path(temp) / "probe.c"
        source.write_text(probe_program())
        executable = Path(temp) / "probe"
        subprocess.run([compiler, "-std=c11", "-w", optimization, str(source), "-o", str(executable)],
                       check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
