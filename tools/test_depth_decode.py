"""The stored depth's view distance: one decode, checked numerically.

The class: a depth decoded with another convention than the one that stored it. The game's projection is GL-style
(normalized depth -p22 + p32 / d), the screen conversion takes it as the 0-1 range without remapping, and Metal
stores its reverse, so a distance d is stored as 1 - (-p22 + p32 / d). Decoding that as a near-to-far range of the
frustum's near and far halved every distance past a couple of world units in ambient occlusion and soft particles.
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = (ROOT / "port/ios/host/gpu_metal.m").read_text()
HEADER = (ROOT / "port/linux/src/depth_decode.h").read_text()

NEAR, FAR = 0.0078125, 1024.0
P22 = -(FAR + NEAR) / (FAR - NEAR)
P32 = -2.0 * NEAR * FAR / (FAR - NEAR)


def stored(distance):
    return 1.0 - (-P22 + P32 / distance)


def shared_decode():
    macro = re.search(r"#define DEPTH_VIEW_DISTANCE_MSL(.*?)\n\n", HEADER, re.S).group(1)
    expression = re.search(r"return (.*?);", macro).group(1)
    return lambda s, p22, p32: eval(expression, {"stored": s, "p22": p22, "p32": p32})


def test_the_shared_decode_returns_the_true_distance():
    decode = shared_decode()
    for distance in (0.05, 0.5, 3.0, 40.0, 900.0):
        assert abs(decode(stored(distance), P22, P32) / distance - 1.0) < 1e-6


def test_the_old_near_to_far_decode_was_half_the_distance():
    for distance in (3.0, 40.0, 200.0):
        d = 1.0 - stored(distance)
        old = NEAR * FAR / (FAR - d * (FAR - NEAR))
        assert 0.49 < old / distance < 0.56


def test_every_program_that_reads_the_depth_uses_the_shared_decode():
    assert SOURCE.count("\n\tDEPTH_VIEW_DISTANCE_MSL\n") == 2  # the two programs
    assert "depth_view_distance(stored, p.depth_p22, p.depth_p32)" in SOURCE
    assert "depth_view_distance(stored, p.x, p.y)" in SOURCE
    assert "(p.z_far - d * (p.z_far - p.z_near))" not in SOURCE
    assert "p.y - (1.0 - stored)" not in SOURCE


def test_the_depth_row_falls_back_to_the_frustum_row():
    assert "-(far_plane + near_plane) / (far_plane - near_plane)" in HEADER
    assert "-2.0f * near_plane * far_plane / (far_plane - near_plane)" in HEADER
    assert '#include "depth_decode.h"' in SOURCE
