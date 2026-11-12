"""The blended camera stays one tick apart after a frame that runs two ticks."""
from pathlib import Path
import shutil
import subprocess
import tempfile

import pytest

from test_checkpoint_timing import function

ROOT = Path(__file__).resolve().parents[1]

PROGRAM = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef float real;
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define _pi 3.14159265358979f
#define CAMERA_CUT_DISTANCE 3.0f
#define CAMERA_CUT_COSINE 0.5f
typedef struct { real x, y, z; } real_point3d;
typedef struct { real i, j, k; } real_vector3d;
struct observer_result { real_point3d position; real_vector3d forward, up; real field_of_view; };
struct interpolated_camera {
    long tick; boolean valid, has_previous;
    struct observer_result previous, latest;
    real previous_head_yaw, latest_head_yaw;
    boolean previous_facing_posed, latest_facing_posed;
};
/* FUNCTIONS */
static struct interpolated_camera camera;
static real speed = 0.4f; /* world units a tick: a warthog at speed */
/* the snapshot step of render_interpolation_blended_camera */
static void keep(long tick, real x)
{
    if (!camera.valid || camera.tick != tick) {
        long ticks = camera.valid ? tick - camera.tick : 0;
        camera.has_previous = camera.valid;
        camera.previous = camera.latest;
        camera.latest.position.x = x;
        camera.latest.position.y = camera.latest.position.z = 0;
        camera.latest.forward.i = 1; camera.latest.forward.j = camera.latest.forward.k = 0;
        camera.latest.up.i = camera.latest.up.j = 0; camera.latest.up.k = 1;
        camera.latest.field_of_view = 1.2f;
        camera.previous_facing_posed = camera.latest_facing_posed;
        camera.tick = tick; camera.valid = TRUE;
        camera_previous_one_tick_back(&camera, ticks);
    }
}
int main(void)
{
    /* ticks run before each frame: 60 fps with a 30 Hz tick, one frame late */
    int cadence[] = {1, 0, 1, 0, 2, 0, 0, 1, 0, 3, 0, 1, 0};
    real t[] = {0.0f, 0.5f, 0.0f, 0.5f, 0.1f, 0.6f, 0.9f, 0.0f, 0.5f, 0.2f, 0.7f, 0.0f, 0.5f};
    long tick = 10;
    for (int frame = 0; frame < (int)(sizeof cadence / sizeof *cadence); frame++) {
        tick += cadence[frame];
        keep(tick, tick * speed);
        if (!camera.has_previous) continue;
        /* the vehicle: its two snapshots one tick apart */
        real vehicle = lerp((tick - 1) * speed, tick * speed, t[frame]);
        real eye = lerp(camera.previous.position.x, camera.latest.position.x, t[frame]);
        printf("frame %d tick %ld lead %f\n", frame, tick, vehicle - eye);
        assert(fabsf(vehicle - eye) < 1e-4f);
        assert(fabsf(camera.previous.forward.i - 1) < 1e-6f);
    }
    /* a cut (teleport) keeps the earlier camera as it was */
    keep(tick + 2, 100.0f);
    assert(camera.previous.position.x == tick * speed);
    puts("ok");
    return 0;
}
'''


@pytest.mark.skipif(not shutil.which("cc"), reason="needs a C compiler")
def test_camera_previous_is_one_tick_old():
    source = (ROOT / "port/linux/game/render_interpolation.c").read_text()
    names = ["lerp", "point_lerp", "vector_length", "vector_nlerp",
             "distance_squared", "vector_turn_about_up", "camera_previous_one_tick_back"]
    program = PROGRAM.replace("/* FUNCTIONS */", "\n".join(function(source, n) for n in names))
    with tempfile.TemporaryDirectory() as directory:
        c = Path(directory) / "camera.c"
        binary = Path(directory) / "camera"
        c.write_text(program)
        subprocess.run(["cc", "-std=gnu11", "-Wall", "-Werror", "-Wno-unused-function",
                        str(c), "-lm", "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        assert result.stdout.endswith("ok\n")
