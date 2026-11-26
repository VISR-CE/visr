"""A level pictures frame that is not a picture never wraps to another map's picture."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "port/linux/game/custom_edition_maps.c").read_text()
STOCK = (ROOT / "port/runtime/stock_maps.h").read_text()


def picture_function():
    body = SOURCE[SOURCE.index("struct bitmap_data *custom_edition_maps_picture("):]
    return body[:body.index("\n}\n")]


def test_only_stock_frames_keep_the_stock_picture():
    # one positive rule on the level pictures tag: frames 0..UNKNOWN_LEVEL_FRAME keep the stock picture, every other
    # frame goes through the custom map lookup and ends at the unknown frame (no range left to wrap)
    body = picture_function()
    keep = body.index("if (*frame_index >= 0 && *frame_index <= UNKNOWN_LEVEL_FRAME)")
    lookup = body.index("custom_edition_map_get(*frame_index)")
    assert keep < lookup
    assert body[keep:lookup].count("return NULL;") == 1
    assert "*frame_index = UNKNOWN_LEVEL_FRAME;" in body[lookup:lookup + 80]
    # the tag check precedes it, so other bitmaps' frames are left alone
    assert body.index("csstrcasecmp(tag_get_name(bitmap_tag_index), LEVEL_PICTURES_TAG_NAME))", body.index("CAMPAIGN_LEVEL_PICTURES_TAG_NAME")) < keep


def test_unknown_frame_follows_the_last_stock_picture():
    frames = [int(m) for m in re.findall(r'", (\d+) \},?\n', STOCK)]
    assert frames and max(frames) + 1 == int(re.search(r"#define UNKNOWN_LEVEL_FRAME (\d+)", SOURCE).group(1))
