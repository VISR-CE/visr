#!/usr/bin/env python3
"""Mirror the staged Pages site's signed feeds to the feeds host, and prove both hosts serve the same bytes.

`stage` copies the `packs/` tree of a staged site (the output of `tools/pages_asset_overlay.py`) into a directory
that is published as the feeds host's site. `verify` downloads every file of that tree from both hosts and fails
unless each is byte-identical to the staged copy. See docs/data-packs.md, "Feed hosts".
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import sys
import time
import urllib.error
import urllib.request

if __package__:
    from . import feed_hosts
else:
    import feed_hosts

MAX_FILES = 1024
MAX_FILE = 128 * 1024 * 1024
MAX_TOTAL = 1024 * 1024 * 1024
NOJEKYLL = ".nojekyll"
CNAME = "CNAME"


class NoRedirect(urllib.request.HTTPRedirectHandler):
    """Released apps refuse redirects, so a redirecting feed URL counts as unavailable."""

    def redirect_request(self, *_args: object, **_kwargs: object) -> None:
        return None


def tree(root: Path) -> dict[str, bytes]:
    """Every file under root/packs as {path relative to root: bytes}. Symbolic links and odd files are errors."""
    packs = root / "packs"
    if not packs.is_dir() or packs.is_symlink():
        raise ValueError("Staged site has no packs directory")
    result: dict[str, bytes] = {}
    total = 0
    for current, directories, files in os.walk(packs, followlinks=False):
        for name in directories:
            if (Path(current) / name).is_symlink():
                raise ValueError("Feed directories cannot be symbolic links")
        for name in files:
            path = Path(current) / name
            info = path.lstat()
            if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= MAX_FILE:
                raise ValueError("Feed files must be bounded regular files: " + str(path.relative_to(root)))
            total += info.st_size
            result[path.relative_to(root).as_posix()] = path.read_bytes()
    if not 1 <= len(result) <= MAX_FILES or total > MAX_TOTAL:
        raise ValueError("Staged feeds exceed their bounded size")
    receipt = result.get("packs/tester/publication.json")
    if receipt is None:
        raise ValueError("Staged feeds are missing their publication receipt")
    json.loads(receipt)
    return result


def stage(site: Path, destination: Path) -> int:
    files = tree(site)
    if destination.exists():
        raise ValueError("Feeds staging destination already exists")
    destination.mkdir(parents=True)
    for name, data in files.items():
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    (destination / NOJEKYLL).write_bytes(b"")
    (destination / CNAME).write_text(feed_hosts.FEEDS_HOST + "\n")
    return len(files)


def fetch(url: str, attempts: int, delay: float) -> bytes | None:
    """The body at url, or None when it is unavailable after every attempt."""
    for attempt in range(attempts):
        try:
            request = urllib.request.Request(url, headers={"Cache-Control": "no-cache"})
            with urllib.request.build_opener(NoRedirect()).open(request, timeout=60) as response:
                return response.read(MAX_FILE + 1)
        except (urllib.error.URLError, TimeoutError):
            if attempt + 1 < attempts:
                time.sleep(delay)
    return None


def verify(site: Path, *, attempts: int = 40, delay: float = 15.0) -> None:
    """Fail unless every staged feed file is served, byte-identical, by both hosts. Pages takes a short while to
    publish a push, so a mismatch is retried until the attempts run out."""
    files = tree(site)
    problems = []
    for base in (feed_hosts.FEEDS_BASE, feed_hosts.LEGACY_BASE):
        for name, data in sorted(files.items()):
            url = base + "/" + name
            body = fetch(url, attempts, delay)
            # a previous file's wait already covered propagation, so later files need only one quick retry
            attempts = min(attempts, 3)
            if body is None:
                problems.append("unavailable: " + url)
            elif body != data:
                problems.append(
                    "differs: " + url + " (served " + hashlib.sha256(body).hexdigest()[:12]
                    + ", staged " + hashlib.sha256(data).hexdigest()[:12] + ")"
                )
    if problems:
        raise ValueError("Feed hosts do not match the staged site:\n" + "\n".join(problems))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    staging = commands.add_parser("stage")
    staging.add_argument("site", type=Path)
    staging.add_argument("destination", type=Path)
    checking = commands.add_parser("verify")
    checking.add_argument("site", type=Path)
    checking.add_argument("--attempts", type=int, default=40)
    checking.add_argument("--delay", type=float, default=15.0)
    args = parser.parse_args()
    try:
        if args.command == "stage":
            print(json.dumps({"files": stage(args.site, args.destination), "host": feed_hosts.FEEDS_HOST}))
        else:
            verify(args.site, attempts=args.attempts, delay=args.delay)
            print(json.dumps({"verified": True, "hosts": [feed_hosts.FEEDS_HOST, feed_hosts.LEGACY_HOST]}))
    except ValueError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1) from error


if __name__ == "__main__":
    main()
