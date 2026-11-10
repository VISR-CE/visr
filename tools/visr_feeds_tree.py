#!/usr/bin/env python3
"""A checkout of the published feeds, VISR-CE/visr-feeds, for tests and probes.

Feeds do not live in this repository. `VISR_FEEDS_TREE` names a checkout to use as it is; otherwise the commit pinned in
tools/visr-feeds.pin is fetched into build/visr-feeds/<sha> (one commit, no history, blobs on demand) and reused on later
runs. The default tree leaves out the legacy catalog-1 recordings-*.json envelopes (about 300 MB), which only the
receipt's full validation reads; `full=True` fetches everything. `extra` adds paths (relative to the tree, for example
one legacy envelope a probe reads).

Usage: visr_feeds_tree.py [--full] [--extra PATH ...]   print the tree's path
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PIN = ROOT / "tools/visr-feeds.pin"
REMOTE = "https://github.com/VISR-CE/visr-feeds.git"
# what the tests and probes read without the legacy envelopes
SPARSE = [
    "/tester/latest.json", "/tester/publication.json", "/tester/publication-format.txt", "/tester/shaders.json",
    "/tester/recordings-2-*.json", "/tester/profiles/", "/tester/device-profiles-*/", "/tester/recording-blobs/",
    "/tester/shader-maps/",
]


def pinned() -> str:
    sha = PIN.read_text().strip()
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError(f"{PIN} must hold one full commit SHA")
    return sha


def git(folder: Path, *arguments: str, stdin: str | None = None) -> None:
    result = subprocess.run(["git", "-C", str(folder), *arguments], input=stdin, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"git {' '.join(arguments)} failed ({result.returncode}): {result.stderr.strip()}")


def feeds_tree(full: bool = False, extra: tuple[str, ...] = ()) -> Path:
    override = os.environ.get("VISR_FEEDS_TREE")
    if override:
        tree = Path(override).resolve()
        if not (tree / "tester").is_dir():
            raise ValueError(f"VISR_FEEDS_TREE {tree} has no tester/ folder")
        return tree
    sha = pinned()
    patterns = [] if full else SPARSE + ["/" + path.lstrip("/") for path in extra]
    folder = ROOT / "build/visr-feeds" / (sha + ("-full" if full else ""))
    marker = folder / ".fetched"
    wanted = "\n".join(patterns)
    if marker.is_file() and marker.read_text() == wanted:
        return folder
    folder.mkdir(parents=True, exist_ok=True)
    if not (folder / ".git").exists():
        git(folder, "init", "--quiet")
        git(folder, "remote", "add", "origin", REMOTE)
    git(folder, "fetch", "--quiet", "--depth", "1", "--filter=blob:none", "origin", sha)
    if not full:
        git(folder, "sparse-checkout", "set", "--no-cone", "--stdin", stdin=wanted + "\n")
    git(folder, "checkout", "--quiet", "--force", sha)
    marker.write_text(wanted)
    return folder


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--full", action="store_true")
    parser.add_argument("--extra", action="append", default=[], help="a path in the tree to include besides the default set")
    arguments = parser.parse_args()
    try:
        print(feeds_tree(arguments.full, tuple(arguments.extra)))
    except (ValueError, RuntimeError, subprocess.CalledProcessError, OSError) as error:
        print(f"visr_feeds_tree: {error}", file=sys.stderr)
        sys.exit(1)
