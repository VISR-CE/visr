"""The Mac app's unsynchronized frames (display.frame_rate "unlimited" or a cap
above the display's rate) wait for a drawable like every other frame, so none
is drawn without being shown."""
from pathlib import Path
import re

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "port/ios/host/gpu_metal.m"


def test_unsynchronized_frames_wait_for_their_drawable():
    source = SOURCE.read_text()
    # the window's frame takes its drawable (the other match is iOS's system frame pacing)
    takes = re.findall(r"drawable = frame_held \|\| gave_up \? nil :\s*([^;]+);", source)
    assert takes == ["system_frame_take()", "[layer nextDrawable]"], takes
    # no thread fetches drawables ahead for frames that would otherwise go unshown
    assert "Metal drawables" not in source
    assert "free_drawable" not in source


def test_unsynchronized_frames_still_leave_the_refresh():
    sync = probe_source.function(SOURCE, "frame_rate_sync")
    assert "layer.displaySyncEnabled = sync;" in sync
    assert "sync = !host_frame_rate_unlimited()" in sync


if __name__ == "__main__":
    test_unsynchronized_frames_wait_for_their_drawable()
    test_unsynchronized_frames_still_leave_the_refresh()
    print("mac unsynchronized present: ok")
