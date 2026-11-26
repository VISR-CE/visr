"""The stored depth's view distance: one decode, checked against the depth the game really stores.

The class: a depth decoded with another convention than the one that stored it. The game's projection row is 0-to-1
near to far (source/render/render_cameras.c: p22 = -f / (f - n), p32 = -f n / (f - n)), so clip z / w is 0 at the
near plane and 1 at the far one, and the vertex translator (nv2a_vsh.c) stores the reverse, (w - z) / w. Decoding that
as a near-to-far range of the frustum's near and far, or deriving the near and far from the row with GL formulas,
halved every distance past a couple of world units in ambient occlusion and soft particles. Every test here stores a
depth the way the game does and decodes it the way the code does.
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
HEADER = (ROOT / "port/linux/src/depth_decode.h").read_text()

CAMERAS = (ROOT / "source/render/render_cameras.c").read_text()
NEAR, FAR = 0.0625, 1024.0
DISTANCES = (0.07, 0.5, 3.0, 40.0, 200.0, 900.0)


def game_row(near, far):
    """The projection's depth row as render_cameras.c builds it (an unskewed view: clip_offset = near)."""
    scale = far / (far - near)
    return -scale, -scale * near


def stored(distance, p22, p32):
    """What the vertex translator writes: (w - z) / w, for clip z = p22 * -d + p32 and w = d."""
    return 1.0 - (-p22 * distance + p32) / distance


def decode_expression():
    macro = re.search(r"#define DEPTH_VIEW_DISTANCE_MSL(.*?)\n\n", HEADER, re.S).group(1)
    expression = re.search(r"return (.*?);", macro).group(1)
    return lambda s, p22, p32: eval(expression, {"stored": s, "p22": p22, "p32": p32})


def fallback_row(near, far, row22=1.0, row32=0.0):
    """depth_row_from_view, as written in the header."""
    if row22 != 1.0 and row22 != -1.0 and row32 != 0.0:
        return row22, row32
    body = re.findall(r"\*p22 = (.*?);\s*\*p32 = (.*?);", HEADER, re.S)[-1]  # the fallback, after the early return
    names = {"far_plane": far, "near_plane": near}
    return eval(body[0], names), eval(body[1], names)


def test_the_row_in_this_test_is_the_one_the_game_builds():
    assert "frustum->projection_matrix[2][2] = -view_plane.n.k;" in CAMERAS
    assert "frustum->projection_matrix[3][2] = view_plane.d;" in CAMERAS
    assert "view_plane.d = -(projection_scale * clip_offset);" in CAMERAS
    p22, p32 = game_row(NEAR, FAR)
    assert abs(stored(NEAR, p22, p32) - 1.0) < 1e-12 and abs(stored(FAR, p22, p32)) < 1e-12


def test_the_shared_decode_returns_the_true_distance():
    decode = decode_expression()
    p22, p32 = game_row(NEAR, FAR)
    for distance in DISTANCES:
        assert abs(decode(stored(distance, p22, p32), p22, p32) / distance - 1.0) < 1e-6


def test_the_fallback_row_is_the_games_row():
    decode = decode_expression()
    p22, p32 = fallback_row(NEAR, FAR)
    assert (abs(p22 - game_row(NEAR, FAR)[0]) < 1e-12) and (abs(p32 - game_row(NEAR, FAR)[1]) < 1e-12)
    for distance in DISTANCES:
        assert abs(decode(stored(distance, *game_row(NEAR, FAR)), p22, p32) / distance - 1.0) < 1e-6
    assert fallback_row(NEAR, FAR, -1.001, -0.5) == (-1.001, -0.5)  # a row the projection gave is kept


def test_the_old_decode_with_gl_derived_planes_was_wrong_for_this_depth():
    """12196c12b derived the near and far from the row with GL formulas (p32 / (p22 -+ 1)), which gives n / 2 for the
    game's 0-to-1 row, then read the reverse depth as a near-to-far range of them."""
    p22, p32 = game_row(NEAR, FAR)
    near_gl, far_gl = p32 / (p22 - 1.0), p32 / (p22 + 1.0)
    assert abs(near_gl / NEAR - 0.5) < 0.01 and abs(far_gl / FAR - 1.0) < 1e-9
    for distance in (3.0, 40.0, 200.0):
        d = 1.0 - stored(distance, p22, p32)
        old = near_gl * far_gl / (far_gl - d * (far_gl - near_gl))
        assert 0.49 < old / distance < 0.56


def test_every_reader_of_a_depth_target_uses_the_shared_decode():
    """Every file under port/ that decodes a stored depth: no near-to-far shape anywhere, and the shader sources that
    rebuild a distance emit or call the shared one."""
    shapes = ("(p.z_far - d * (p.z_far - p.z_near))", "p.y - (1.0 - stored)", "soft.y - (1.0 - soft_position.z)")
    for path in (ROOT / "port").rglob("*"):
        if path.suffix not in (".c", ".m", ".h") or path.name == "depth_decode.h":
            continue
        text = path.read_text(errors="ignore")
        for shape in shapes:
            assert shape not in text, (path, shape)
    metal = (ROOT / "port/ios/host/gpu_metal.m").read_text()
    assert metal.count("\n\tDEPTH_VIEW_DISTANCE_MSL\n") == 2  # ambient occlusion, soft particle distances
    assert "depth_view_distance(stored, p.depth_p22, p.depth_p32)" in metal
    assert "depth_view_distance(stored, p.x, p.y)" in metal
    assert "DEPTH_VIEW_DISTANCE_MSL;" in (ROOT / "port/linux/src/nv2a_msl.c").read_text()  # in every MSL shader
    assert "depth_view_distance(soft_position.z, soft.x, soft.y)" in (ROOT / "port/linux/src/nv2a_psh.c").read_text()
    assert "depth_view_distance(light_position.z, traced_light.depth_p22, traced_light.depth_p32)" in (
        ROOT / "port/linux/src/nv2a_psh.c").read_text()  # the traced light's depth point
    stereo = (ROOT / "port/ios/host/host_stereo.m").read_text()
    assert "depth_view_distance_c(depth, p22, p32)" in stereo and "near * far / (depth" not in stereo


def test_the_stereo_hud_depth_is_the_same_distance():
    """host_stereo.m's old hand-written inverse, near * far / (depth * (far - near) + near), equals the shared decode
    of the fallback row at the game's stored depth."""
    p22, p32 = fallback_row(NEAR, FAR)
    for distance in DISTANCES:
        depth = stored(distance, *game_row(NEAR, FAR))
        old = NEAR * FAR / (depth * (FAR - NEAR) + NEAR)
        assert abs(old / distance - 1.0) < 1e-6
        assert abs(p32 / (p22 + 1.0 - depth) / distance - 1.0) < 1e-6
