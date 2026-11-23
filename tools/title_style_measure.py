#!/usr/bin/env python3
"""Measure the title pictures once to choose live titles' standard values.

Live menu titles (spec: menu titles as live text, step 5) are drawn with one color, one cap height, one baseline, one
left edge and one background treatment. This reads the header pictures in `port/assets/titles/` (the disc headers set
again at 2x or 4x) and reports, in menu units, each picture's text color, cap height, baseline and left edge, and
whether the background under the text is empty, a glow that follows the letters or a plate; then the most common
value of each, which becomes the title element's constant.

    python3 tools/title_style_measure.py [--json PATH]
"""

import argparse
import collections
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TITLES = ROOT / "port/assets/titles"


def dilate(mask, radius):
    """mask grown by radius texels (a square: the measure needs near and far, not exact distances)"""
    grown = mask.copy()
    padded = np.pad(mask, radius)
    for dy in range(-radius, radius + 1, max(1, radius // 4)):
        for dx in range(-radius, radius + 1, max(1, radius // 4)):
            grown |= padded[radius + dy:radius + dy + mask.shape[0], radius + dx:radius + dx + mask.shape[1]]
    return grown


def measure(entry):
    image = np.asarray(Image.open(TITLES / f"{entry['name']}.png").convert("RGBA")).astype(float)
    scale = entry["scale"]
    rgb, alpha = image[..., :3], image[..., 3]
    value = rgb.max(axis=-1)
    # the text: the bright texels (a header's text is bright blue over a dark plate or a glow)
    text = (alpha > 128) & (value >= 0.6 * value[alpha > 128].max())
    rows = np.flatnonzero(text.any(axis=1))
    columns = np.flatnonzero(text.any(axis=0))
    color = np.median(rgb[text], axis=0)
    # cap height: the rows most letters cover (descenders and punctuation aside), from the column-wise extents
    tops, bottoms = [], []
    for column in columns:
        filled = np.flatnonzero(text[:, column])
        tops.append(filled[0])
        bottoms.append(filled[-1])
    top = collections.Counter(tops).most_common(1)[0][0]
    bottom = collections.Counter(bottoms).most_common(1)[0][0]
    # the background: alpha away from the text, near it (within 3 menu units) and far from it
    near = dilate(text, 3 * scale) & ~dilate(text, scale // 2)
    far = ~dilate(text, 6 * scale)
    near_alpha = float(alpha[near].mean()) if near.any() else 0.0
    far_alpha = float(alpha[far].mean()) if far.any() else 0.0
    if far_alpha > 24:
        background = "plate"
    elif near_alpha > 24:
        background = "glow"
    else:
        background = "empty"
    return {
        "name": entry["name"],
        "text": entry["text"],
        "color": "#%02X%02X%02X" % tuple(int(round(c)) for c in color),
        "cap": round((bottom - top + 1) / scale, 2),
        "baseline": round((bottom + 1) / scale, 2),
        "left": round(columns[0] / scale, 2),
        "near_alpha": round(near_alpha, 1),
        "far_alpha": round(far_alpha, 1),
        "background": background,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--json", type=Path, help="write the per-picture values and the chosen constants here")
    arguments = parser.parse_args()
    entries = [entry for entry in json.loads((TITLES / "titles.json").read_text())["assets"]
               if "header" in entry["name"]]
    measured = [measure(entry) for entry in entries]
    chosen = {}
    for key in ("color", "cap", "baseline", "left", "background"):
        values = collections.Counter(item[key] for item in measured)
        chosen[key] = values.most_common(1)[0][0]
        print(f"{key}: {chosen[key]}  ({dict(values.most_common(5))})")
    for item in measured:
        print(f"  {item['text']:<28} {item['color']} cap {item['cap']:>5} base {item['baseline']:>5} "
              f"left {item['left']:>5} {item['background']} (near {item['near_alpha']}, far {item['far_alpha']})")
    if arguments.json:
        arguments.json.write_text(json.dumps({"chosen": chosen, "pictures": measured}, indent=2) + "\n")


if __name__ == "__main__":
    main()
