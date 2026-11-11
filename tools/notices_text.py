#!/usr/bin/env python3
"""Generate the bundled notices: python3 tools/notices_text.py [--check]."""
from pathlib import Path
import argparse

import about_data

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "port/ios/Licenses/NOTICES.txt"


def generate():
    sections = ["VISR — Open Source Notices"]
    for name, license, copyright, file, _ in about_data.LICENSES:
        text = (ROOT / file).read_text(encoding="utf-8").strip()
        # The streamer list's README includes its MIT license in a fenced block.
        if name == "STREAMER WORDS":
            text = text.split("```\n", 1)[1].split("```", 1)[0].strip()
        sections.append(f"{name}\n{license}\n{copyright}\n\n{text}")
    return "\n\n".join(sections) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check the committed notices without writing")
    args = parser.parse_args()
    expected = generate()
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text(encoding="utf-8") != expected:
            parser.exit(1, "NOTICES.txt is stale; run python3 tools/notices_text.py\n")
    else:
        OUTPUT.write_text(expected, encoding="utf-8")


if __name__ == "__main__":
    main()
