"""A level pictures frame that is not a picture never wraps to another map's picture."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "port/linux/game/custom_edition_maps.c").read_text()


def picture_function():
    body = SOURCE[SOURCE.index("struct bitmap_data *custom_edition_maps_picture("):]
    return body[:body.index("\n}\n")]


def test_out_of_range_frames_show_the_unknown_level():
    body = picture_function()
    clamp = body.index("*frame_index = UNKNOWN_LEVEL_FRAME;\n\t\treturn NULL;")
    guard = body[:clamp]
    # the guard names the level pictures tag and both ends of the range, and runs before the custom-map lookup
    assert "LEVEL_PICTURES_TAG_NAME" in guard[guard.rindex("if (bitmap_tag_index"):]
    assert "*frame_index < 0" in guard and "UNKNOWN_LEVEL_FRAME" in guard and "FIRST_CAMPAIGN_DISPLAY_INDEX" in guard
    assert clamp < body.index("custom_edition_map_get(*frame_index)")


def test_unknown_frame_is_inside_the_stock_pictures():
    # the unknown frame stays the "?" after the 13 stock maps' pictures (frames 0..12)
    assert re.search(r"#define UNKNOWN_LEVEL_FRAME 13\b", SOURCE)
