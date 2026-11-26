"""A map picture render is silent: one hold on the mixer, which every mixing path honors."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIXER = (ROOT / "port/linux/src/dsound_sdl.c").read_text()
RENDER = (ROOT / "port/linux/src/map_pictures_render.c").read_text()


def test_mixer_paths_use_the_muted_output_volume():
    # master_volume may be read in its setters and in output_volume() only: a path that reads it
    # directly (the loading screen's tone buffers once did) is not silenced by the hold.
    body = MIXER[MIXER.index("static float output_volume(void)"):]
    body = body[:body.index("}")]
    assert "capture_mute" in body and "master_volume" in body
    outside = re.sub(r"/\*.*?\*/", "", MIXER.replace(body, ""), flags=re.S)
    for match in re.finditer(r"master_volume", outside):
        line = outside[outside.rfind("\n", 0, match.start()) + 1:outside.index("\n", match.end())]
        assert re.search(r"master_volume\s*=|static float master_volume|capture_mute\s*\?", line), line


def test_every_render_run_holds_the_mixer_silent():
    start = RENDER[RENDER.index("host_map_pictures_keep_awake(1);"):]
    start = start[:start.index("wait_menu();")]
    assert "dsound_set_capture_mute(1);" in start
    assert "if (render.import_mode)\n\t\tdsound_set_capture_mute" not in RENDER
    # released when the run ends
    leaving = RENDER[RENDER.index("case STATE_LEAVING:"):]
    assert "dsound_set_capture_mute(0);" in leaving[:leaving.index("case STATE_DONE:")]


def test_renderer_version_is_a_plain_constant():
    # meta.json's renderer_version decides whether every cached picture is re-rendered: it is bumped by hand when the
    # renderer's output changes, never derived from the build number, commit, date or version string.
    lines = [l for l in RENDER.splitlines() if l.startswith("#define RENDERER_VERSION")]
    assert len(lines) == 1 and re.fullmatch(r"#define RENDERER_VERSION \d+", lines[0].strip()), lines
