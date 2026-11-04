"""tools/embed_title_replacements.py: the menu titles the game draws over the originals, as the upscaler's table."""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TITLES = ROOT / "port/assets/titles/titles.json"


def emit(tmp_path, raw):
    source = tmp_path / "titles.json"
    source.write_text(json.dumps(raw))
    out = tmp_path / "table.c"
    subprocess.run([sys.executable, str(ROOT / "tools/embed_title_replacements.py"), str(source), str(out)], check=True)
    return out.read_text()


def test_every_shipped_title_is_listed_with_its_checksum(tmp_path):
    assets = json.loads(TITLES.read_text())["assets"]
    text = emit(tmp_path, json.loads(TITLES.read_text()))
    rows = re.findall(r'\{ "((?:[^"\\]|\\.)*)", (\d+), 0x([0-9a-f]{8})u \}', text)
    assert len(rows) == len(assets) == int(re.search(r"texture_title_replacement_count = (\d+)", text).group(1))
    for row, asset in zip(rows, assets):
        assert row[0].replace("\\\\", "\\") == asset["tag"]
        assert int(row[1]) == asset["bitmap"] and int(row[2], 16) == asset["crc"]


def test_no_titles_leave_a_valid_empty_table(tmp_path):
    text = emit(tmp_path, {"assets": []})
    assert "{ 0 }," in text and "texture_title_replacement_count = 0" in text
