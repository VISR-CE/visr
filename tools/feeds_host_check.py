#!/usr/bin/env python3
"""Prove a build reads every feed from feeds.visr.gg: stage the bucket's tree (a VISR-CE/visr-feeds checkout, whose
layout is the bucket's), serve it on this machine, and run
port/ios/tests/feeds_host_probe.m against it with the build's key and URLs.

The URLs are the ones a release from this source gets: the current release policy moved to feeds.visr.gg
(feeds_store.py policy). Every file it names must be staged; its hashes are pinned again for each release, so they are
not compared here. The recording feed is the one in the format this source's app reads. Development builds' defaults
must name the same host.

Without --packs the tree is the one tools/visr_feeds_tree.py returns: the commit pinned in tools/visr-feeds.pin, or the
checkout in VISR_FEEDS_TREE. A checkout whose release policy is newer than the tree is reported and skipped; move the
pin, or pass the current VISR-CE/visr-feeds checkout with --packs.
Usage: feeds_host_check.py FEEDS_HOST_PROBE [--packs VISR_FEEDS_CHECKOUT]
"""
from __future__ import annotations

import argparse
import functools
import http.server
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading
import urllib.parse

if __package__:
    from . import feeds_store, ios_build, visr_feeds_tree
    from . import release_validation as validation
else:
    import feeds_store
    import ios_build
    import release_validation as validation
    import visr_feeds_tree

ROOT = Path(__file__).resolve().parents[1]
FEED_HOST = urllib.parse.urlsplit(feeds_store.FEEDS_URL).netloc


class Stale(ValueError):
    """The staged tree lacks a file the release policy names."""


def key_of(url: str) -> str:
    """The bucket key a feeds.visr.gg URL names."""
    parts = urllib.parse.urlsplit(url)
    if parts.scheme != "https" or parts.netloc != FEED_HOST:
        raise ValueError("Not a feeds.visr.gg URL: " + url)
    return urllib.parse.unquote(parts.path).lstrip("/")


def release_urls(staged: Path) -> tuple[str, str, str, str]:
    """Profile directory, shader feed, recording feed and its compatibility for a release from this source."""
    policy = feeds_store.move_policy(json.loads((ROOT / ".release/current.json").read_text())["build_policy"])
    validation.load_policy(json.dumps(policy))

    def staged_at(url: str) -> None:
        if not (staged / key_of(url)).is_file():
            raise Stale("The bucket's tree does not hold a file the release policy names: " + url)

    staged_at(policy["shader_feed_url"])
    for target in policy["profile_feed_sha256s"]:
        staged_at(policy["profile_feed_url"].rstrip("/") + "/" + target + ".json")
    # the catalog format this source's app accepts (host_data_updates.m), and the feed of that format
    catalog = re.search(r'@"benchmark-catalog-([12]):" HALO_GUEST_SHA256', (ROOT / "port/ios/host/host_data_updates.m").read_text())
    if not catalog:
        raise ValueError("Cannot find the recording catalog format the app reads")
    recording, guest = policy["recording_feed_url"], policy["guest_sha256"]
    if catalog[1] == "1":
        staged_at(recording)
    else:
        feeds = sorted((staged / "tester").glob("recordings-2-*.json"))
        if not feeds:
            raise ValueError("No benchmark-catalog-2 feed is staged")
        guest = feeds[-1].stem.removeprefix("recordings-2-")
        recording = feeds_store.FEEDS_URL + "/tester/" + feeds[-1].name
    return policy["profile_feed_url"], policy["shader_feed_url"], recording, f"benchmark-catalog-{catalog[1]}:{guest}"


def development_defaults() -> None:
    args = argparse.Namespace(data_pack_public_key=None, profile_feed_url=None, recording_feed_url=None,
                              shader_feed_url=None, data_packs="on", mac=False, tester=True, data_pack_channel=None)
    _, *urls = ios_build.data_pack_settings(args)
    for url in urls:
        key_of(url)


def copy_packs(packs: Path, staged: Path) -> None:
    for channel in ("tester", "staging"):
        source = packs / channel
        for path in sorted(source.rglob("*")) if source.is_dir() else ():
            if path.is_symlink():
                raise ValueError("Feed files cannot be symbolic links: " + str(path))
            if path.is_file():
                target = staged / path.relative_to(packs)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(path.read_bytes())
    if not (staged / "tester/publication.json").is_file():
        raise ValueError(f"{packs} is not a feeds tree (no tester/publication.json)")


def serve(folder: Path) -> http.server.ThreadingHTTPServer:
    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *args):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Quiet, directory=str(folder)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def main(arguments=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("probe", type=Path)
    parser.add_argument("--packs", type=Path, help="a VISR-CE/visr-feeds checkout, staged as it is (default: the pinned tree)")
    args = parser.parse_args(arguments)
    development_defaults()
    with tempfile.TemporaryDirectory(prefix="feeds-host-") as temporary:
        staged = Path(temporary) / "bucket"
        # The files as they are: the probe checks each signature with the app's own code, and the Tooling suite checks
        # that feeds_store.py uploads exactly these keys (ios_test's Python has no cryptography package).
        copy_packs(args.packs or visr_feeds_tree.feeds_tree(), staged)
        try:
            profile, shaders, recording, compatibility = release_urls(staged)
        except Stale as error:
            if args.packs:
                raise
            print(f"SKIP: {error}; this checkout's release policy is newer than tools/visr-feeds.pin. "
                  "Move the pin, or run tools/feeds_host_check.py with --packs <visr-feeds checkout>.")
            return 0
        server = serve(staged)
        try:
            result = subprocess.run([str(args.probe), FEED_HOST, str(server.server_address[1]),
                                     ios_build.DATA_PACK_PUBLIC_KEY, profile, shaders, recording, compatibility])
        finally:
            server.shutdown()
            server.server_close()
    return result.returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, KeyError) as error:
        print(f"feeds_host_check: {error}", file=sys.stderr)
        sys.exit(1)
