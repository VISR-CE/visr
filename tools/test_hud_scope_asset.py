"""The sniper's aim ring and both Xbox zoom labels must ship together."""

import json
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

from tools import embed_assets


ROOT = Path(__file__).resolve().parents[1]


def test_sniper_scope_sheet_is_embedded_at_8x():
    assets = json.loads((ROOT / "port/assets/hud/layout.json").read_text())["assets"]
    scope = [entry for entry in assets if entry["tag"] == r"ui\hud\bitmaps\sniper\hud_reticles_scope"]
    assert len(scope) == 1
    entry = scope[0]
    assert (entry["bitmap"], entry["width"], entry["height"], entry["format"], entry["scale"]) == \
        (0, 64, 64, "a8y8", 8)
    assert entry["crc"] == 2373183232
    # Sequence 0 is the aim ring; sequence 1 has the 2x and 10x labels.
    assert [cell["xbox"] for cell in entry["cells"]] == \
        [[32, 0, 50, 18], [0, 0, 31, 24], [0, 24, 43, 48]]
    for cell in entry["cells"]:
        assert cell["kind"] == "alpha"
        assert cell["source_scale"] == 1
        assert cell["source"] == cell["xbox"][:2]
        assert cell["clip"] == cell["xbox"]
        svg = ROOT / "port/assets/hud/svg" / cell["svg"]
        root = ET.parse(svg).getroot()
        assert root.get("viewBox") == "0 0 64 64"
        assert not root.findall(".//{http://www.w3.org/2000/svg}image")
        ring = root.find("{http://www.w3.org/2000/svg}circle")
        assert ring is not None
        assert [float(ring.get(key)) for key in ("cx", "cy", "r", "stroke-width")] == [40.5, 8.5, 4, 1]
    png = Path("port/assets/hud") / f"{entry['name']}.png"
    assert png in embed_assets.hud_asset_inputs()
    data = (ROOT / png).read_bytes()
    assert data[:8] == embed_assets.PNG_SIGNATURE
    assert struct.unpack_from(">II", data, 16) == (512, 512)


def test_sniper_scope_center_dot_is_round():
    root = ET.parse(ROOT / "port/assets/hud/svg/hud__bitmaps__sniper__hud_reticles_scope.svg").getroot()
    dot = root.findall("{http://www.w3.org/2000/svg}circle")[1]
    assert [float(dot.get(key)) for key in ("cx", "cy", "r")] == [40.5, 8.5, 0.6]
    assert dot.get("fill") == "#fff"
