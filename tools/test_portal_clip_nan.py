"""Clip a portal outline whose corner sits exactly on the clip line and check no corner becomes NaN."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def function(source, name):
    match = re.search(r"^(?:static )?[\w *]+\b" + name + r"\(\s*[^;{]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError(f"Function not found: {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


PRELUDE = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef float real;
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define CEILING(n,ceiling) ((n)>(ceiling)?(ceiling):(n))
#define PIN(n,floor,ceiling) ((n)<(floor) ? (floor) : CEILING((n),(ceiling)))
#define FLAG(b) (1<<(b))
#define TEST_FLAG(f, b) ((f)&FLAG(b))
#define SET_FLAG(f, b, v) ((v) ? ((f)|=(unsigned)FLAG(b)) : ((f)&=(unsigned)~FLAG(b)))
#define CLIP_BUFFER_SIZE 64
#define NUMBER_OF_VERTICES_PER_TRIANGLE 3
#define match_assert(file, line, condition) assert(condition)
#define csmemcpy memcpy
typedef struct { real x, y; } real_point2d;
typedef struct { real i, j; } real_vector2d;
typedef struct { real_vector2d n; real d; } real_plane2d;
static real_point2d *point_from_line2d(real_point2d const *p, real_vector2d const *v, real t, real_point2d *result)
{ result->x = t*v->i + p->x; result->y = t*v->j + p->y; return result; }
static real_vector2d *vector_from_points2d(real_point2d const *a, real_point2d const *b, real_vector2d *result)
{ result->i = b->x-a->x; result->j = b->y-a->y; return result; }
static real plane2d_distance_to_point(real_plane2d const *plane, real_point2d const *point)
{ return (plane->n.i*point->x + plane->n.j*point->y) - plane->d; }
'''

# Each case is a plane and a corner p exactly on it, with its neighbor q a rounding error behind
# (both terms of the crossing parameter round to zero), found by a random search with the
# engine's unfused float arithmetic. r is a third corner well in front of the plane.
TESTS = r'''
struct nan_case { real nx, ny, d, px, py, qx, qy; };
static const struct nan_case cases[] = {
    {0.996336639f, -0.0855173022f, 0.56391269f, 0.510330498f, -0.648426652f, 0.595543206f, 0.344361246f},
    {-0.989110053f, 0.147177532f, 0.76443094f, -1.09860575f, -2.18926811f, -0.900981545f, -0.861130059f},
    {-0.904920936f, 0.425579786f, -0.529958606f, 0.478770584f, -0.227240801f, -0.060829863f, -1.37460673f},
};
int main(void) {
    int i, c;
    for (c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); c++) {
        const struct nan_case *k = &cases[c];
        real_plane2d plane = {{k->nx, k->ny}, k->d};
        real_point2d p = {k->px, k->py}, q = {k->qx, k->qy};
        real_point2d r = {k->px + k->nx * 0.5f, k->py + k->ny * 0.5f};
        real_point2d points[3] = {p, r, q}; /* p's predecessor is q */
        real_point2d result[CLIP_BUFFER_SIZE];
        real_vector2d v;
        short count;
        boolean clipped;
        /* the premise: the unclamped crossing parameter really is NaN here */
        assert(plane2d_distance_to_point(&plane, &p) >= 0.f && plane2d_distance_to_point(&plane, &q) < 0.f);
        vector_from_points2d(&p, &q, &v);
        {
            real t = vector_intersect_plane2d(&p, &v, &plane);
            assert(t != t);
        }
        count = convex_polygon2d_clip_to_plane(3, points, &plane, CLIP_BUFFER_SIZE, result, NULL, &clipped, 0.f);
        assert(count >= 3);
        assert(clipped);
        for (i = 0; i < count; i++) {
            assert(isfinite(result[i].x) && isfinite(result[i].y));
        }
        /* a second clip of the result, as the portal walk does, stays finite */
        count = convex_polygon2d_clip_to_plane(count, result, &plane, CLIP_BUFFER_SIZE, result, NULL, &clipped, 0.f);
        for (i = 0; i < count; i++) {
            assert(isfinite(result[i].x) && isfinite(result[i].y));
        }
    }
    puts("PASS: portal clip never yields NaN corners");
    return 0;
}
'''


def main():
    source = (ROOT / "source/math/geometry.c").read_text()
    names = ["vector_intersect_plane2d", "convex_polygon2d_clip_to_plane"]
    code = PRELUDE + "\n".join(function(source, name) for name in names) + TESTS
    with tempfile.TemporaryDirectory(prefix="portal-clip-nan-") as tmp:
        path = Path(tmp) / "test.c"
        binary = Path(tmp) / "test"
        path.write_text(code)
        # the engine builds without fused multiply-add (tools/ios_guest_build.py); the NaN needs the unfused form
        subprocess.run(["xcrun", "clang", "-std=c11", "-O0", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
                        str(path), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
