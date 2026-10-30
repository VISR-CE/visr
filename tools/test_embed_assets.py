"""Embedded fonts retain their optional variant and share each file's data."""
import json
import sys

from tools import embed_assets


def test_font_variants(tmp_path, monkeypatch):
    folder = tmp_path / embed_assets.FONT_ASSETS
    folder.mkdir(parents=True)
    fonts = [
        {"tag": "ui\\large_ui", "file": "plain.ttf"},
        {"tag": "ui\\small_ui", "file": "plain.ttf", "variant": ""},
        {"tag": "ui\\large_ui", "file": "redraw.ttf", "variant": "redraw"},
    ]
    (tmp_path / embed_assets.FONT_LIST).write_text(json.dumps({"fonts": fonts}))
    for name in ("plain.ttf", "redraw.ttf"):
        (folder / name).write_bytes(b"font")
    output = tmp_path / "assets.c"
    monkeypatch.setattr(embed_assets, "ROOT", tmp_path)
    monkeypatch.setattr(sys, "argv", ["embed_assets.py", str(output)])
    embed_assets.main()
    generated = output.read_text()
    assert '{ "ui\\\\large_ui", "plain.ttf", font0, 4, 0 }' in generated
    assert '{ "ui\\\\small_ui", "plain.ttf", font0, 4, "" }' in generated
    assert '{ "ui\\\\large_ui", "redraw.ttf", font1, 4, "redraw" }' in generated
    assert generated.count("static const unsigned int font") == 2
    assert "text_hires_embedded_count = 3;" in generated
    assert embed_assets.FONT_ASSETS / "redraw.ttf" in embed_assets.hud_asset_inputs()
