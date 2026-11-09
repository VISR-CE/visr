"""Benchmark screenshot assets and the build rows that link to them."""

import csv
from html.parser import HTMLParser
import json
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from perf_compare import charts, site_page

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "docs/site"
SHOTS = SITE / "benchmarks/shots"
DATA = json.loads((SHOTS / "shots.json").read_text(encoding="utf-8"))


class BuildRows(HTMLParser):
    def __init__(self):
        super().__init__()
        self.rows = []
        self.row = None
        self.depth = 0
        self.in_label = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "div":
            if "build" in attrs.get("class", "").split():
                self.row = {"shots": [], "images": []}
                self.depth = 0
            if self.row is not None:
                self.depth += 1
        if self.row is None:
            return
        if tag == "label":
            self.in_label = True
        if tag == "input":
            self.row["key"] = attrs["value"]
        if tag == "a" and attrs.get("class") == "build-shot":
            assert not self.in_label
            self.row["shots"].append(attrs)
        if tag == "img":
            self.row["images"].append(attrs)

    def handle_endtag(self, tag):
        if tag == "label":
            self.in_label = False
        if tag == "div" and self.row is not None:
            self.depth -= 1
            if self.depth == 0:
                self.rows.append(self.row)
                self.row = None


def test_screenshot_assets_exist_fit_budgets_and_use_known_scenes():
    scenes = {scene["key"] for scene in DATA["scenes"]}
    with (ROOT / "docs/benchmarks/results.csv").open(newline="", encoding="utf-8") as source:
        released = {f'{row["port"]}@{row["version"]}' for row in csv.DictReader(source)
                    if row["release_kind"] in ("release", "pinned source build")}
    for key, build in DATA["builds"].items():
        assert "@".join(key.split("@")[:2]) in released
        assert key.endswith("@" + build["device_id"])
        assert set(build["scenes"]) <= scenes
        assert build["thumb_scene"] in build["scenes"]
        assert (SHOTS / build["thumb"]).is_file()
        assert (SHOTS / build["thumb"]).stat().st_size < 8_000
        for scene, file in build["scenes"].items():
            assert (SHOTS / file).is_file()
            assert (SHOTS / file).stat().st_size < 160_000
            assert len(build["size"][scene]) == 2
            assert all(value > 0 for value in build["size"][scene])
    assert sum(file.stat().st_size for file in SHOTS.rglob("*") if file.is_file()) < 8_000_000


def test_rendered_build_rows_link_only_to_available_screenshots():
    rows = charts.load_snapshot(charts.SNAPSHOT)
    html = site_page.render_site_page({}, rows, charts.load_specs(charts.SPECS, rows), "")
    assert 'data-shots="benchmarks/shots/shots.json"' in html
    parsed = BuildRows()
    parsed.feed(html)
    assert parsed.rows
    matched = 0
    for row in parsed.rows:
        if not row["shots"]:
            assert not row["images"]
            continue
        matched += 1
        [link], [image] = row["shots"], row["images"]
        key = link["data-shot"]
        build = DATA["builds"][key]
        assert "@".join(row["key"].split("@")[:2]) == "@".join(key.split("@")[:2])
        assert link["aria-label"].startswith(f'Screenshots of {build["label"]}, Campaign')
        assert f'Captured on {build["device"]}' in link["title"]
        scene = next(x for x in DATA["scenes"] if x["key"] == link["data-scene"])
        assert scene["suite"] == "Campaign"
        assert link["href"] == "benchmarks/shots/" + build["scenes"][scene["key"]]
        assert (SITE / link["href"]).is_file()
        assert image["src"] == link["href"]
        assert (image["width"], image["height"], image["loading"], image["decoding"], image["alt"]) == ("56", "42", "lazy", "async", "")
    assert matched


@pytest.mark.parametrize("off", [None, {"reason": "not measured", "tag": "not measured"}])
def test_build_screenshots_need_the_rows_own_device_port_and_version(off):
    entry = {"port": "visr", "version": "0.6.11", "build": "different", "date": "2026-11-06", "label": "VISR 0.6.11"}
    parsed = BuildRows()
    parsed.feed(site_page.build_item(entry, {"exact": {}, "patterns": {}}, off, shots=DATA, device="Mac mini|M6", suite="campaign"))
    assert len(parsed.rows[0]["shots"]) == 1
    for other in ("Mac mini|M1", "Apple TV 4K|A15", ""):
        parsed = BuildRows()
        parsed.feed(site_page.build_item(entry, {"exact": {}, "patterns": {}}, off, shots=DATA, device=other, suite="campaign"))
        assert not parsed.rows[0]["shots"]
    entry["version"] = "unavailable"
    parsed = BuildRows()
    parsed.feed(site_page.build_item(entry, {"exact": {}, "patterns": {}}, off, shots=DATA, device="Mac mini|M6", suite="campaign"))
    assert not parsed.rows[0]["shots"]


def test_python_and_javascript_screenshot_markup_contract():
    python = (ROOT / "tools/perf_compare/site_page.py").read_text(encoding="utf-8")
    javascript = (SITE / "benchmarks/benchmarks.js").read_text(encoding="utf-8")
    for name in ("build-shot", "data-shot", "aria-label", "loading", "decoding", "data-scene"):
        assert name in python and name in javascript
    assert 'width="56" height="42"' in python
    assert 'width: "56", height: "42"' in javascript


def test_thumbnail_follows_the_selected_suite_and_needs_a_frame_in_it():
    entry = {"port": "visr", "version": "0.6.11", "build": "x", "date": "2026-11-06", "label": "VISR 0.6.11"}
    reasons = {"exact": {}, "patterns": {}}
    build = DATA["builds"]["visr@0.6.11@Mac mini|M6"]
    by_suite = {}
    for scene in DATA["scenes"]:
        by_suite.setdefault(scene["suite"], []).append(scene["key"])
    for suite, name in charts.SUITE_NAMES.items():
        parsed = BuildRows()
        parsed.feed(site_page.build_item(entry, reasons, None, shots=DATA, device="Mac mini|M6", suite=suite))
        have = [k for k in by_suite.get(name, []) if k in build["scenes"]]
        if suite in charts.SUITES and have:
            [link] = parsed.rows[0]["shots"]
            assert link["data-scene"] == have[0]
        else:
            assert not parsed.rows[0]["shots"], suite
    # A build with no frame for the suite shows nothing.
    trimmed = {**DATA, "builds": {"visr@0.6.11@Mac mini|M6": {**build, "scenes": {"c10": build["scenes"]["c10"]}}}}
    parsed = BuildRows()
    parsed.feed(site_page.build_item(entry, reasons, None, shots=trimmed, device="Mac mini|M6", suite="multiplayer"))
    assert not parsed.rows[0]["shots"]
    javascript = (SITE / "benchmarks/benchmarks.js").read_text(encoding="utf-8")
    assert "state.suite)" in javascript and "function shotScene" in javascript
