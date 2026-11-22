"""tools/shader_lists.py: merging recorded shader lists into port/shader-lists"""
import struct

import pytest

from tools import shader_lists


def fnv(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f"{value:016x}"


def key_hex(count=0, junk=0, fog=0):
    """a 252-byte pixel key: `count` combiner stages, `junk` in every register of the stages from `count` on, and the
    fog_enable byte"""
    words = [0] * 57
    words[53] = count
    for base in (0, 26, 34, 45):
        for stage in range(8):
            words[base + stage] = 0x11 + stage if stage < count else junk
    body = struct.pack("<57I", *words) + bytes(24)
    return (body[:248] + bytes([fog]) + body[249:]).hex()


def ps_line(key=None):
    key = key or key_hex()
    return f"ps {fnv(bytes.fromhex(key))} {key}"


PS_HASH = ps_line().split()[1]
VS = "vs 00000000000000aa 0 0"
PS = ps_line()
PIPELINE = f"pipeline 00000000000000aa.0.0 {PS_HASH} 0 0 0 0 15 0 1 " + "00" * 16 + " 0 0"
# the same pipeline before lines carried the separate alpha factors
OLD_PIPELINE = f"pipeline 00000000000000aa.0.0 {PS_HASH} 0 0 0 0 15 0 1 " + "00" * 16
# a blended pipeline's line without alpha factors, and the same one with them (alpha as color)
OLD_BLENDED = f"pipeline 00000000000000aa.0.0 {PS_HASH} 1 4 5 0 15 0 1 " + "00" * 16
BLENDED = OLD_BLENDED + " 4 5"
# the stereo HUD layer's twin of it: color as before, alpha by ZERO and the destination's factor
HUD_LAYER = OLD_BLENDED + " 0 5"
CLOSED = [VS, PS, PIPELINE]


def entries(path):
    return [line for line in path.read_text().splitlines() if line and not line.startswith("#")]


def test_a_run_folder_adds_its_lines_in_kind_order(tmp_path):
    lists = tmp_path / "lists"
    recorded = tmp_path / "run/runner/shader-lists"
    recorded.mkdir(parents=True)
    (recorded / "a10.txt").write_text(f"{PIPELINE}\n{PS}\n{VS}\n")
    shader_lists.merge([tmp_path / "run"], lists)
    assert entries(lists / "a10.txt") == [VS, PS, PIPELINE]
    assert (lists / "a10.txt").read_text().startswith("# a10: ")


def test_a_folder_of_map_files_merges_as_it_is(tmp_path):
    """the app's shader-lists-missed folder, pulled from a device or a runner"""
    lists = tmp_path / "lists"
    missed = tmp_path / "shader-lists-missed"
    missed.mkdir()
    (missed / "b30.txt").write_text(f"{VS}\n")
    shader_lists.merge([missed], lists)
    assert entries(lists / "b30.txt") == [VS]


def test_lines_already_listed_or_repeated_are_not_added_again(tmp_path):
    lists = tmp_path / "lists"
    lists.mkdir()
    (lists / "a10.txt").write_text(f"# header\n{VS}\n")
    first, second = tmp_path / "one", tmp_path / "two"
    for folder in (first, second):
        folder.mkdir()
        (folder / "a10.txt").write_text(f"{VS}\n{PS}\n{PS}\n")
    added = shader_lists.merge([first, second], lists)
    assert entries(lists / "a10.txt") == [VS, PS]
    assert added == {"a10": 1}


def test_comments_and_blanks_are_dropped_but_an_unknown_kind_is_refused(tmp_path):
    lists = tmp_path / "lists"
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "c40.txt").write_text(f"# comment\n\n  \n{PS}  \n")
    shader_lists.merge([missed], lists)
    assert entries(lists / "c40.txt") == [PS]
    (missed / "c40.txt").write_text(f"bogus 1 2 3\n{PS}\n")
    with pytest.raises(SystemExit) as refused:
        shader_lists.merge([missed], lists)
    assert "kind: not vs, ps or pipeline" in str(refused.value)


def test_a_folder_with_no_lists_is_an_error(tmp_path):
    try:
        shader_lists.merge([tmp_path], tmp_path / "lists")
    except SystemExit as error:
        assert "no recorded lists" in str(error)
    else:
        raise AssertionError("merge accepted a folder with no lists")


def test_an_old_pipeline_line_reads_as_blending_alpha_as_color(tmp_path):
    """lines from before the alpha factors stay valid and merge with their new form"""
    lists = tmp_path / "lists"
    lists.mkdir()
    (lists / "a10.txt").write_text(f"# header\n{VS}\n{PS}\n{OLD_PIPELINE}\n{OLD_BLENDED}\n")
    recorded = tmp_path / "missed"
    recorded.mkdir()
    (recorded / "a10.txt").write_text(f"{PIPELINE}\n{BLENDED}\n{HUD_LAYER}\n")
    added = shader_lists.merge([recorded], lists)
    assert added == {"a10": 1}
    assert sorted(entries(lists / "a10.txt")) == sorted([VS, PS, PIPELINE, BLENDED, HUD_LAYER])


def test_the_hud_layer_pipeline_is_not_its_mono_twin(tmp_path):
    lists = tmp_path / "lists"
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "b30.txt").write_text(f"{VS}\n{PS}\n{BLENDED}\n{HUD_LAYER}\n")
    shader_lists.merge([missed], lists)
    assert len([line for line in entries(lists / "b30.txt") if line.startswith("pipeline")]) == 2


def test_a_ps_key_is_canonical_and_its_pipelines_follow_its_hash(tmp_path):
    clean, dirty = key_hex(2, 0), key_hex(2, 0xdead)
    pipeline = "pipeline 00000000000000aa.0.0 {} 0 0 0 0 15 0 1 " + "00" * 16 + " 0 0"
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "a10.txt").write_text("\n".join([VS, ps_line(dirty), pipeline.format(fnv(bytes.fromhex(dirty)))]) + "\n")
    shader_lists.merge([missed], tmp_path / "lists")
    assert entries(tmp_path / "lists/a10.txt") == [VS, ps_line(clean), pipeline.format(fnv(bytes.fromhex(clean)))]


def test_two_keys_that_differ_only_past_the_combiner_count_are_one_shader(tmp_path):
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "a10.txt").write_text(f"{ps_line(key_hex(2, 0xdead))}\n{ps_line(key_hex(2, 0xbeef))}\n")
    shader_lists.merge([missed], tmp_path / "lists")
    assert len(entries(tmp_path / "lists/a10.txt")) == 1


def test_stages_below_the_combiner_count_are_kept(tmp_path):
    missed = tmp_path / "missed"
    missed.mkdir()
    full = ps_line(key_hex(8, 0xdead))
    (missed / "a10.txt").write_text(full + "\n")
    shader_lists.merge([missed], tmp_path / "lists")
    assert entries(tmp_path / "lists/a10.txt") == [full]


REFUSED = {
    "a bad hash": (f"ps 1111111111111111 {key_hex()}\n", "hash"),
    "an out-of-range field": (ps_line(key_hex()[:464] + "06" + key_hex()[466:]) + "\n", "sampler_type[0]=6, allowed 0-5"),
    "a pipeline with no pixel shader anywhere": (f"{VS}\n{PIPELINE}\n", "unresolved pipeline"),
}


@pytest.mark.parametrize("name", REFUSED)
def test_a_bad_file_exits_nonzero_and_leaves_the_lists_as_they_were(tmp_path, name):
    lists = tmp_path / "lists"
    lists.mkdir()
    (lists / "a10.txt").write_text("# header\n" + "\n".join(CLOSED) + "\n")
    (lists / "b30.txt").write_text(f"# header\n{VS}\n")
    before = {path.name: path.read_bytes() for path in lists.iterdir()}
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "b30.txt").write_text(REFUSED[name][0])
    (missed / "c20.txt").write_text(f"{PS}\n")
    with pytest.raises(SystemExit) as refused:
        shader_lists.merge([missed], lists)
    assert REFUSED[name][1] in str(refused.value)
    assert {path.name: path.read_bytes() for path in lists.iterdir()} == before


def test_a_pipeline_is_closed_by_the_list_it_joins(tmp_path):
    lists = tmp_path / "lists"
    lists.mkdir()
    (lists / "a10.txt").write_text("# header\n" + f"{PS}\n")
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "a10.txt").write_text(f"{PIPELINE}\n")
    shader_lists.merge([missed], lists)
    assert entries(lists / "a10.txt") == CLOSED


def test_canonicalize_rewrites_a_noncanonical_list_and_is_a_no_op_after(tmp_path):
    lists = tmp_path / "lists"
    lists.mkdir()
    (lists / "a10.txt").write_text("# header\n" + f"{ps_line(key_hex(2, 0xdead))}\n")
    assert shader_lists.canonicalize(lists) == {"a10": (1, 1)}
    first = (lists / "a10.txt").read_bytes()
    assert entries(lists / "a10.txt") == [ps_line(key_hex(2, 0))]
    shader_lists.canonicalize(lists)
    assert (lists / "a10.txt").read_bytes() == first


def test_the_bundled_lists_are_a_canonical_no_op():
    before = {path.name: path.read_bytes() for path in shader_lists.LISTS.glob("*.txt")}
    import tempfile
    from pathlib import Path
    with tempfile.TemporaryDirectory() as temporary:
        staged = Path(temporary) / "out"
        shader_lists.run_tool(["canonical", "--out", str(staged), str(shader_lists.LISTS)], shader_lists.LISTS)
        assert {path.name: path.read_bytes() for path in staged.glob("*.txt")} == before
