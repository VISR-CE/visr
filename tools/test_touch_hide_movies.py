"""The touch controls show only while a map is being played: hidden from launch, over menus, movies, loading, and cutscenes."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def _read(path):
    return (ROOT / path).read_text()


def _update_function():
    source = _read("port/linux/src/d3d8_device.c")
    start = source.index("int halo_ui_pointer_update(int menus_active, struct halo_ui_pointer *pointer)\n{\n\tstatic int pause_menu")
    return source, source[start:source.index("\n}\n", start)]


def test_overlay_covered_by_movie_or_loading_bar():
    source, _ = _update_function()
    body = re.search(r"int halo_touch_overlay_covered\(void\)\s*\{(.*?)\}", source, re.S).group(1)
    assert "bink_playback_in_progress()" in body
    assert "progress_bar_is_active()" in body


def test_guest_publishes_positive_gameplay_predicate():
    _, update = _update_function()
    assert "!menus_active && !halo_touch_overlay_covered() && halo_scene_class() == CONFIG_SCENE_GAMEPLAY" in update
    assert 'host_sdl_set_hint("VISR_TOUCH_GAMEPLAY", gameplay ? "1" : "0")' in update
    # The menu hint stays the menus' own state.
    assert 'host_sdl_set_hint("VISR_MENU_ACTIVE", menu_up ? "1" : "0")' in update
    assert "halo_touch_overlay_covered() ? " not in update


def test_host_defaults_hidden_until_guest_says_gameplay():
    touch = _read("port/ios/host/host_touch.m")
    assert "static atomic_int gameplay_active;" in touch
    assert "atomic_load(&gameplay_active)==0" in touch
    assert "BOOL menu=touch_overlay_off();" in touch
    begin = touch.index("touchesBegan")
    assert "touch_overlay_off()" in touch[begin:begin + 900]
    point = touch.index("pointInside")
    assert "touch_overlay_off()" in touch[point:point + 300]
    assert "void host_ios_touch_gameplay_changed(int gameplay)" in touch


def test_host_forwards_hint_and_stubs_exist():
    sdl = _read("port/ios/host/host_sdl.c")
    assert 'SDL_AddHintCallback("VISR_TOUCH_GAMEPLAY", touch_gameplay_hint, NULL)' in sdl
    assert "host_ios_touch_gameplay_changed" in sdl
    assert "void host_ios_touch_gameplay_changed(int gameplay);" in _read("port/ios/host/ios_host.h")
    assert "host_ios_touch_gameplay_changed" in _read("port/ios/host/host_tv.m")
