"""The touch controls stay hidden over full-screen movies and loading bars, not only menus."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def _update_function():
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    start = source.index("int halo_ui_pointer_update(int menus_active, struct halo_ui_pointer *pointer)\n{\n\tstatic int pause_menu")
    return source, source[start:source.index("\n}\n", start)]


def test_overlay_covered_by_movie_or_loading_bar():
    source, _ = _update_function()
    match = re.search(r"int halo_touch_overlay_covered\(void\)\s*\{(.*?)\}", source, re.S)
    assert match
    body = match.group(1)
    assert "bink_playback_in_progress()" in body
    assert "progress_bar_is_active()" in body


def test_menu_hint_published_from_covered_state():
    _, update = _update_function()
    assert "menus_active != 0 || halo_touch_overlay_covered()" in update
    assert 'host_sdl_set_hint("VISR_MENU_ACTIVE", menu_up ? "1" : "0")' in update
    # Menu pointer bookkeeping keeps following the real menu state.
    assert "platform_menus_set_active(menus_active != 0)" in update
