"""A map picture render is silent: one hold on the mixer, which every mixing path honors."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIXER = (ROOT / "port/linux/src/dsound_sdl.c").read_text()
RENDER = (ROOT / "port/linux/src/map_pictures_render.c").read_text()


def test_the_hold_is_applied_to_the_finished_output():
    # mix() scales the finished output by the hold, after every source (voices, buses, tone buffers) and before the limiter:
    # a source that never reads the hold cannot bypass it. No source reads capture_mute itself.
    mix = MIXER[MIXER.index("static void mix(float *output"):]
    mix = mix[:mix.index("\n}\n")]
    assert mix.index("mix_buffers(output, frames)") < mix.index("capture_mute") < mix.index("limit(output, frames)")
    outside = re.sub(r"/\*.*?\*/", "", MIXER.replace(mix, ""), flags=re.S)
    for match in re.finditer(r"\bcapture_mute\b", outside):
        line = outside[outside.rfind("\n", 0, match.start()) + 1:outside.index("\n", match.end())]
        assert re.search(r"static volatile int capture_mute|capture_mute = on", line), line


def test_every_render_run_holds_the_mixer_silent():
    start = RENDER[RENDER.index("run_hold(1);"):]
    start = start[:start.index("wait_menu();")]
    assert "run_hold(1);" in start
    hold = RENDER[RENDER.index("static void run_hold(int on)"):]
    assert "dsound_set_capture_mute(on);" in hold[:hold.index("}")]
    # released when the run ends
    leaving = RENDER[RENDER.index("case STATE_LEAVING:"):]
    assert "run_hold(0);" in leaving[:leaving.index("case STATE_DONE:")]


def test_renderer_version_is_a_plain_constant():
    # meta.json's renderer_version decides whether every cached picture is re-rendered: it is bumped by hand when the
    # renderer's output changes, never derived from the build number, commit, date or version string.
    lines = [l for l in RENDER.splitlines() if l.startswith("#define RENDERER_VERSION")]
    assert len(lines) == 1 and re.fullmatch(r"#define RENDERER_VERSION \d+", lines[0].strip()), lines
