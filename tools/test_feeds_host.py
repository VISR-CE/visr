"""The feeds host mirrors the staged site's signed feeds and must serve the same bytes as the legacy host."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Self

import pytest

from tools import feed_hosts, feeds_host, release_validation as validation


def staged(tmp_path: Path) -> Path:
    site = tmp_path / "site"
    (site / "packs/tester/device-profiles-3/ios/iPhone18,1").mkdir(parents=True)
    (site / "packs/tester/publication.json").write_text(json.dumps({"schema": 1, "assets": {}}))
    (site / "packs/tester/shaders.json").write_bytes(b"shaders")
    (site / "packs/tester/device-profiles-3/ios/iPhone18,1/_.json").write_bytes(b"profile")
    (site / "index.html").write_text("not a feed")
    return site


def test_legacy_url_maps_only_feeds_host_packs() -> None:
    assert feed_hosts.legacy_url("https://feeds.visr.gg/packs/tester/shaders.json") == (
        "https://code.steverice.org/visr/packs/tester/shaders.json"
    )
    assert feed_hosts.legacy_url("https://FEEDS.visr.gg./packs/tester/a%2Cb.json") == (
        "https://code.steverice.org/visr/packs/tester/a%2Cb.json"
    )
    for url in (
        "https://code.steverice.org/visr/packs/tester/shaders.json",
        "http://feeds.visr.gg/packs/tester/shaders.json",
        "https://feeds.visr.gg/other/shaders.json",
        "https://feeds.visr.gg:8443/packs/tester/shaders.json",
        "https://user@feeds.visr.gg/packs/tester/shaders.json",
        "https://example.test/packs/tester/shaders.json",
    ):
        assert feed_hosts.legacy_url(url) is None


def test_stage_copies_only_packs_and_adds_domain_files(tmp_path: Path) -> None:
    site = staged(tmp_path)
    destination = tmp_path / "feeds"
    assert feeds_host.stage(site, destination) == 3
    assert (destination / "packs/tester/shaders.json").read_bytes() == b"shaders"
    assert (destination / "CNAME").read_text() == "feeds.visr.gg\n"
    assert (destination / ".nojekyll").exists()
    assert not (destination / "index.html").exists()
    with pytest.raises(ValueError, match="already exists"):
        feeds_host.stage(site, destination)


def test_stage_refuses_missing_receipt_and_symbolic_links(tmp_path: Path) -> None:
    site = staged(tmp_path)
    (site / "packs/tester/publication.json").unlink()
    with pytest.raises(ValueError, match="publication receipt"):
        feeds_host.stage(site, tmp_path / "one")
    site = staged(tmp_path / "again")
    (site / "packs/tester/link.json").symlink_to(site / "packs/tester/shaders.json")
    with pytest.raises(ValueError, match="regular files"):
        feeds_host.stage(site, tmp_path / "two")


def serve(monkeypatch: pytest.MonkeyPatch, bodies: dict[str, bytes]) -> None:
    monkeypatch.setattr(feeds_host, "fetch", lambda url, _attempts, _delay: bodies.get(url))


def both(site: Path) -> dict[str, bytes]:
    bodies = {}
    for name, data in feeds_host.tree(site).items():
        bodies[feed_hosts.FEEDS_BASE + "/" + name] = data
        bodies[feed_hosts.LEGACY_BASE + "/" + name] = data
    return bodies


def test_verify_passes_when_both_hosts_serve_the_staged_bytes(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    site = staged(tmp_path)
    serve(monkeypatch, both(site))
    feeds_host.verify(site)


def test_verify_reports_missing_and_different_files(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    site = staged(tmp_path)
    bodies = both(site)
    bodies[feed_hosts.LEGACY_BASE + "/packs/tester/shaders.json"] = b"old"
    del bodies[feed_hosts.FEEDS_BASE + "/packs/tester/device-profiles-3/ios/iPhone18,1/_.json"]
    serve(monkeypatch, bodies)
    with pytest.raises(ValueError) as error:
        feeds_host.verify(site)
    assert "differs: https://code.steverice.org/visr/packs/tester/shaders.json" in str(error.value)
    assert "unavailable: https://feeds.visr.gg/packs/tester/device-profiles-3/ios/iPhone18,1/_.json" in str(error.value)


class Response:
    def __init__(self, body: bytes) -> None:
        self.body = body

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_args: object) -> None:
        return None

    def read(self, limit: int) -> bytes:
        return self.body[:limit]


def opener_for(bodies: dict[str, bytes]):
    class Opener:
        def open(self, url: str, timeout: int) -> Response:
            return Response(bodies[url])

    return lambda *_args: Opener()


def test_read_feed_requires_identical_bytes_on_the_legacy_host(monkeypatch: pytest.MonkeyPatch) -> None:
    new = "https://feeds.visr.gg/packs/tester/shaders.json"
    old = "https://code.steverice.org/visr/packs/tester/shaders.json"
    monkeypatch.setattr(validation.urllib.request, "build_opener", opener_for({new: b"a", old: b"a"}))
    assert validation.read_feed(new, 10, 45) == b"a"
    monkeypatch.setattr(validation.urllib.request, "build_opener", opener_for({new: b"a", old: b"b"}))
    with pytest.raises(ValueError, match="different bytes"):
        validation.read_feed(new, 10, 45)


def test_read_feed_leaves_other_hosts_alone(monkeypatch: pytest.MonkeyPatch) -> None:
    old = "https://code.steverice.org/visr/packs/tester/shaders.json"
    monkeypatch.setattr(validation.urllib.request, "build_opener", opener_for({old: b"a"}))
    assert validation.read_feed(old, 10, 45) == b"a"


def test_workflow_publishes_and_verifies_the_feeds_host() -> None:
    workflow = (Path(__file__).resolve().parents[1] / ".github/workflows/pages.yml").read_text()
    assert "tools/feeds_host.py stage build/pages build/feeds" in workflow
    assert "tools/feeds_host.py verify build/pages" in workflow
    assert "secrets.FEEDS_DEPLOY_KEY" in workflow and "vars.FEEDS_REPOSITORY" in workflow
    assert "StrictHostKeyChecking=yes" in workflow
    assert workflow.index("Publish the feeds host") < workflow.index("actions/deploy-pages")
