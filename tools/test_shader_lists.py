"""tools/shader_lists.py: merging recorded shader lists into port/shader-lists"""
from tools import shader_lists

VS = "vs 00000000000000aa 0 0"
PS = "ps 00000000000000bb " + "00" * 8
PIPELINE = "pipeline 00000000000000aa.0.0 00000000000000bb 0 0 0 0 15 0 1 " + "00" * 16 + " 0 0"
# the same pipeline before lines carried the separate alpha factors
OLD_PIPELINE = "pipeline 00000000000000aa.0.0 00000000000000bb 0 0 0 0 15 0 1 " + "00" * 16
# a blended pipeline's line without alpha factors, and the same one with them (alpha as color)
OLD_BLENDED = "pipeline 00000000000000aa.0.0 00000000000000bb 1 4 5 0 15 0 1 " + "00" * 16
BLENDED = OLD_BLENDED + " 4 5"
# the stereo HUD layer's twin of it: color as before, alpha by ZERO and the destination's factor
HUD_LAYER = OLD_BLENDED + " 0 5"


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


def test_comments_blanks_and_unknown_kinds_are_dropped(tmp_path):
    lists = tmp_path / "lists"
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "c40.txt").write_text(f"# comment\n\n  \nbogus 1 2 3\n{PS}  \n")
    shader_lists.merge([missed], lists)
    assert entries(lists / "c40.txt") == [PS]


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
    (lists / "a10.txt").write_text(f"# header\n{OLD_PIPELINE}\n{OLD_BLENDED}\n")
    recorded = tmp_path / "missed"
    recorded.mkdir()
    (recorded / "a10.txt").write_text(f"{PIPELINE}\n{BLENDED}\n{HUD_LAYER}\n")
    added = shader_lists.merge([recorded], lists)
    assert added == {"a10": 1}
    assert sorted(entries(lists / "a10.txt")) == sorted([PIPELINE, BLENDED, HUD_LAYER])


def test_the_hud_layer_pipeline_is_not_its_mono_twin(tmp_path):
    lists = tmp_path / "lists"
    missed = tmp_path / "missed"
    missed.mkdir()
    (missed / "b30.txt").write_text(f"{BLENDED}\n{HUD_LAYER}\n")
    shader_lists.merge([missed], lists)
    assert len(entries(lists / "b30.txt")) == 2


def key_hex(count, junk):
    """a 252-byte pixel key: `count` combiner stages, and `junk` in every register of the stages from 4 on"""
    import struct
    words = [0] * 57
    words[53] = count
    for base in (0, 26, 34, 45):
        for stage in range(8):
            words[base + stage] = 0x11 + stage if stage < count else junk
    return (struct.pack("<57I", *words) + bytes(24)).hex()


def test_a_ps_key_is_canonical_and_its_pipelines_follow_its_hash():
    clean, dirty = key_hex(2, 0), key_hex(2, 0xdead)
    clean_hash = f"{shader_lists.fnv1a64(bytes.fromhex(clean)):016x}"
    pipeline = "pipeline 00000000000000aa.0.0 {} 0 0 0 0 15 0 1 " + "00" * 16 + " 0 0"
    lines = canonical_of([f"ps 1111111111111111 {dirty}", f"ps 2222222222222222 {clean}", pipeline.format("1111111111111111"),
                          pipeline.format("2222222222222222")])
    assert sorted(lines) == sorted([f"ps {clean_hash} {clean}", pipeline.format(clean_hash)])


def canonical_of(lines):
    return shader_lists.canonical(set(lines))


def test_stages_below_the_combiner_count_are_kept():
    full = key_hex(8, 0xdead)
    assert shader_lists.canonical_key(bytes.fromhex(full)) == bytes.fromhex(full)


def test_a_short_key_is_left_as_it_is():
    assert canonical_of([PS, PIPELINE]) == {PS, PIPELINE}


def test_merging_canonicalizes_what_it_reads(tmp_path):
    lists = tmp_path / "lists"
    recorded = tmp_path / "run/runner/shader-lists"
    recorded.mkdir(parents=True)
    (recorded / "a10.txt").write_text(f"ps 1111111111111111 {key_hex(2, 0xdead)}\nps 2222222222222222 {key_hex(2, 0xbeef)}\n")
    shader_lists.merge([tmp_path / "run"], lists)
    assert len(entries(lists / "a10.txt")) == 1
