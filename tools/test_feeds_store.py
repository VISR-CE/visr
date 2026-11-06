"""The R2 mirror of the checked-in feeds (tools/feeds_store.py): what it uploads, in what order, how it signs, and
that the bucket's tree serves every file byte for byte."""
from __future__ import annotations

import base64
import datetime
import hashlib
import http.server
import json
import os
import re
import subprocess
import sys
from pathlib import Path
import threading

import pytest

from tools import feeds_store as store
from tools import release_validation as validation
from tools import visr_feeds_tree

ROOT = Path(__file__).resolve().parents[1]
overlay = store.overlay
SHADER = "docs/site/packs/tester/shaders.json"
RECORDING = "docs/site/packs/tester/recordings.json"
RECORDING_060 = (
    "docs/site/packs/tester/recordings-"
    "28b14010a886a159afff1bec13ec7f516b5235f270934906f15ff59ff10b0fd1.json"
)
RECORDING_061 = (
    "docs/site/packs/tester/recordings-"
    "c7fe63a3433bb5ef215fbd498679caf5688f56663b65879d496d39981f482b91.json"
)
RECORDING_062 = (
    "docs/site/packs/tester/recordings-"
    "36970174a94fd63fb36d43dadf6011911db23fb49f68291beca46b4d71963c0f.json"
)
RECORDING_063 = (
    "docs/site/packs/tester/recordings-"
    "c2ce7f3b23742865e51ad2716fd5dd15ecfe14c68c205cea5ba5be33e3d00b89.json"
)
RECORDING_065 = (
    "docs/site/packs/tester/recordings-"
    "cd6918f423702083087fddf32473d31340014fb565fd96c90b995dddf43cd350.json"
)
RECORDING_066 = (
    "docs/site/packs/tester/recordings-"
    "af40aee1ede971d28a67d3532084184f8192ea9bb887978807e17a63d3ebe5b1.json"
)
RECORDING_068 = (
    "docs/site/packs/tester/recordings-"
    "884e797b336ee160a71955d02fff76cedf57a42826f369b4636920ca311832b5.json"
)
RECORDING_0611 = (
    "docs/site/packs/tester/recordings-"
    "67c55749538c7f6d52151bd9d5025dea4959ba0374108828f7917cf6fd0c0437.json"
)
RECORDING_0612 = (
    "docs/site/packs/tester/recordings-"
    "814ba59a492ffc4b5aaa1a7d03429f5017ba18aad19b6427e1c1b86b77d33e37.json"
)
RECORDING_0610 = (
    "docs/site/packs/tester/recordings-"
    "73c231e484545195f01d5b86454e8ff6b933b2da5f455adc0d16b8a8ede53edd.json"
)
RECORDING_069 = (
    "docs/site/packs/tester/recordings-"
    "ef68cbd68ddaf5e4bb038488a8521ee381ae44f7653e020e0b0325ceb81da5df.json"
)
RECORDING_067 = (
    "docs/site/packs/tester/recordings-"
    "3b35b4be4f2faa36c7e10fb4e340a5f5d8f599d0290cee5a9b251e7ff8a6c107.json"
)


class Host:
    """A local stand-in for a feed host and the S3 endpoint: GET serves files, PUT records uploads."""

    def __init__(self, files: dict[str, bytes]):
        self.files, self.puts, self.redirects = dict(files), [], set()
        host = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                key = self.path.lstrip("/").replace("%2C", ",")
                if key in host.redirects:
                    self.send_response(302)
                    self.send_header("Location", "https://elsewhere.invalid/" + key)
                    self.end_headers()
                    return
                data = host.files.get(key)
                self.send_response(200 if data is not None else 404)
                self.send_header("Content-Length", str(len(data or b"")))
                self.end_headers()
                self.wfile.write(data or b"")

            def do_PUT(self):
                body = self.rfile.read(int(self.headers["Content-Length"]))
                host.puts.append((self.path, dict(self.headers.items()), body))
                self.send_response(200)
                self.send_header("Content-Length", "0")
                self.end_headers()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.url = f"http://127.0.0.1:{self.server.server_address[1]}"
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()


@pytest.fixture
def host():
    servers = []

    def make(files=None):
        servers.append(Host(files or {}))
        return servers[-1]

    yield make
    for server in servers:
        server.close()


@pytest.fixture(scope="module")
def feeds_tree():
    """The pinned steverice/visr-feeds tree, every file (tools/visr_feeds_tree.py)."""
    return visr_feeds_tree.feeds_tree(full=True)


@pytest.fixture(scope="module")
def checked_in(feeds_tree):
    return store.items_from(feeds_tree)


def synthetic() -> list[store.Item]:
    paths = {
        "docs/site/packs/tester/publication.json": b"{}\n",
        "docs/site/packs/tester/publication-format.txt": b"visr-signed-assets-1\n",
        "docs/site/packs/tester/shaders.json": b"{\"shaders\":1}",
        "docs/site/packs/tester/latest.json": b"{\"latest\":1}",
        "docs/site/packs/tester/device-profiles-4/ios/iPad11,3/_.json": b"{\"shard\":1}",
        "docs/site/packs/tester/shader-maps/" + "a" * 64 + ".zlib": b"map",
        "docs/site/packs/tester/recording-blobs/" + "b" * 64 + ".xz": b"blob",
    }
    return [store.item(path, data) for path, data in paths.items()]


def test_each_key_gets_its_type_cache_policy_and_rank():
    assert store.describe("tester/shader-maps/x.zlib") == ("application/octet-stream", store.IMMUTABLE, 0)
    assert store.describe("tester/recording-blobs/x.xz") == ("application/octet-stream", store.IMMUTABLE, 0)
    assert store.describe("tester/device-profiles-4/ios/iPhone18,1/_.json") == ("application/json", store.MUTABLE, 1)
    assert store.describe("tester/profiles/ios/iPhone18,1/_.json") == ("application/json", store.MUTABLE, 1)
    assert store.describe("tester/recordings-2-" + "c" * 64 + ".json") == ("application/json", store.MUTABLE, 2)
    assert store.describe("tester/publication-format.txt")[2] == 3
    assert store.describe("tester/publication.json")[2] == 4
    assert store.describe("staging/publication.json")[2] == 4
    with pytest.raises(ValueError):
        store.describe("tester/notes.md")
    with pytest.raises(ValueError):
        store.key_for("docs/site/index.html")
    with pytest.raises(ValueError):
        store.key_for("docs/site/packs/tester/../index.html")


def test_uploads_go_archives_first_and_receipts_last():
    order = [entry.key for entry in store.plan(synthetic(), {})]
    assert order[:2] == ["tester/recording-blobs/" + "b" * 64 + ".xz", "tester/shader-maps/" + "a" * 64 + ".zlib"]
    assert order[2] == "tester/device-profiles-4/ios/iPad11,3/_.json"
    assert order[-2:] == ["tester/publication-format.txt", "tester/publication.json"]


def test_only_what_the_bucket_lacks_is_uploaded():
    items = synthetic()
    live = {entry.key: entry.sha256 for entry in items}
    assert store.plan(items, live) == []
    live["tester/shaders.json"] = "0" * 64
    assert [entry.key for entry in store.plan(items, live)] == ["tester/shaders.json"]


def test_the_feeds_tree_mirrors_its_receipt(checked_in, feeds_tree):
    keys = {entry.key for entry in checked_in}
    receipt = json.loads((feeds_tree / "tester/publication.json").read_bytes())["assets"]
    assert {store.key_for(path) for path in receipt} | {"tester/publication.json", "tester/publication-format.txt"} <= keys
    assert all(key.startswith(("tester/", "staging/")) for key in keys)
    staging = [entry for entry in checked_in if entry.key == store.STAGING_RECEIPT]
    if staging:
        assets = json.loads(staging[0].data)["assets"]
        assert assets == {entry.key: entry.sha256 for entry in checked_in if entry.key.startswith("staging/") and entry.key != store.STAGING_RECEIPT}


def test_publish_dry_run_uploads_everything_once_then_nothing(checked_in, host, tmp_path):
    empty = host()
    backend = store.DryRun()
    changes = store.publish(backend, empty.url, checked_in)
    assert backend.puts == changes and len(changes) == len(checked_in)
    assert changes[-1].key == "tester/publication.json"
    # once the bucket serves the staged tree, nothing differs
    store.stage(tmp_path / "bucket", checked_in)
    served = host({entry.key: (tmp_path / "bucket" / entry.key).read_bytes() for entry in checked_in})
    again = store.DryRun()
    assert store.publish(again, served.url, checked_in) == [] and again.puts == []
    assert store.verify(served.url, checked_in) == []


def test_a_live_envelope_is_replaced_only_through_the_rollback_rule(checked_in, host, monkeypatch):
    shaders = next(entry for entry in checked_in if entry.key == "tester/shaders.json")
    files = {entry.key: entry.data for entry in checked_in}
    files["tester/shaders.json"] = b"older shader envelope"
    receipt = json.loads(files["tester/publication.json"])
    receipt["assets"]["docs/site/packs/tester/shaders.json"] = hashlib.sha256(b"older shader envelope").hexdigest()
    files["tester/publication.json"] = json.dumps(receipt).encode()
    served = host(files)
    seen = []

    def advance(path, old, new):
        seen.append((path, old, new))
        raise ValueError("Replacement asset needs a higher signed sequence")

    monkeypatch.setattr(store.overlay, "advance", advance)
    backend = store.DryRun()
    with pytest.raises(ValueError, match="higher signed sequence"):
        store.publish(backend, served.url, checked_in)
    assert seen == [("docs/site/packs/tester/shaders.json", b"older shader envelope", shaders.data)]
    assert backend.puts == []


def test_verify_finds_a_changed_file_and_refuses_redirects(host):
    items = synthetic()
    files = {entry.key: entry.data for entry in items}
    files["tester/latest.json"] = b"tampered"
    served = host(files)
    served.redirects.add("tester/shaders.json")
    with pytest.raises(ValueError, match="must not redirect"):
        store.verify(served.url, items)
    served.redirects.clear()
    assert store.verify(served.url, items) == ["tester/latest.json"]


def test_hosts_are_https_except_this_machine():
    store.require_base("https://feeds.visr.gg")
    store.require_base("http://127.0.0.1:8000")
    for base in ("http://feeds.visr.gg", "https://feeds.visr.gg/?x=1", "ftp://feeds.visr.gg"):
        with pytest.raises(ValueError):
            store.require_base(base)


def test_signature_v4_matches_the_aws_examples():
    # Amazon S3 API reference, "Examples: Signature Calculations in AWS Signature Version 4", GET Object
    now = datetime.datetime(2013, 5, 24, tzinfo=datetime.timezone.utc)
    secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
    empty = hashlib.sha256(b"").hexdigest()
    headers = store.sigv4_headers("GET", "examplebucket.s3.amazonaws.com", "/test.txt", {"Range": "bytes=0-9"}, empty,
                                  access_key="AKIAIOSFODNN7EXAMPLE", secret_key=secret, region="us-east-1", now=now)
    assert headers["authorization"] == (
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;range;x-amz-content-sha256;x-amz-date, "
        "Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41")
    # PUT Object
    body = hashlib.sha256(b"Welcome to Amazon S3.").hexdigest()
    headers = store.sigv4_headers("PUT", "examplebucket.s3.amazonaws.com", "/test%24file.text",
                                  {"Date": "Fri, 24 May 2013 00:00:00 GMT", "x-amz-storage-class": "REDUCED_REDUNDANCY"}, body,
                                  access_key="AKIAIOSFODNN7EXAMPLE", secret_key=secret, region="us-east-1", now=now)
    assert headers["authorization"].endswith("Signature=98ad721746da40c64f1a55b78f14c238d841ea1380cd77a1b5971af0ece108bd")


def test_s3_uploads_are_signed_puts_with_the_feed_headers(host):
    endpoint = host()
    backend = store.S3("0" * 32, "key-id", "secret", endpoint=endpoint.url)
    shard = store.item("docs/site/packs/tester/device-profiles-4/ios/iPad11,3/_.json", b"{\"shard\":1}")
    backend.put(shard)
    path, headers, body = endpoint.puts[0]
    headers = {name.lower(): value for name, value in headers.items()}
    assert path == "/visr-feeds/tester/device-profiles-4/ios/iPad11%2C3/_.json"
    assert body == shard.data
    assert headers["content-type"] == "application/json" and headers["cache-control"] == store.MUTABLE
    assert headers["x-amz-content-sha256"] == shard.sha256
    assert headers["authorization"].startswith("AWS4-HMAC-SHA256 Credential=key-id/")
    assert "/auto/s3/aws4_request" in headers["authorization"]
    with pytest.raises(ValueError, match="R2_ACCOUNT_ID"):
        store.S3("", "key-id", "secret")


def test_wrangler_uploads_name_the_bucket_key_and_headers(tmp_path):
    calls = []
    backend = store.Wrangler(command=["wrangler"], scratch=tmp_path, run=lambda arguments, check: calls.append(arguments))
    blob = store.item("docs/site/packs/tester/shader-maps/" + "a" * 64 + ".zlib", b"map")
    backend.put(blob)
    assert calls == [["wrangler", "r2", "object", "put", "visr-feeds/tester/shader-maps/" + "a" * 64 + ".zlib",
                      f"--file={tmp_path / blob.key}", "--content-type=application/octet-stream",
                      "--cache-control=" + store.IMMUTABLE, "--remote"]]
    assert not (tmp_path / blob.key).exists()


def test_staging_refuses_files_outside_its_patterns(tmp_path, monkeypatch):
    monkeypatch.setattr(store.overlay, "ROOT", tmp_path)
    folder = tmp_path / store.STAGING
    (folder / "recording-blobs").mkdir(parents=True)
    (folder / "recordings.json").write_bytes(b"{}")
    data = b"archive"
    (folder / "recording-blobs" / (hashlib.sha256(data).hexdigest() + ".xz")).write_bytes(data)
    items = store.staging_items()
    assert [entry.key for entry in items][-1] == store.STAGING_RECEIPT
    (folder / "recording-blobs" / ("0" * 64 + ".xz")).write_bytes(data)
    with pytest.raises(ValueError, match="content hash"):
        store.staging_items()
    (folder / "recording-blobs" / ("0" * 64 + ".xz")).unlink()
    (folder / "notes.txt").write_bytes(b"x")
    with pytest.raises(ValueError, match="Unexpected"):
        store.staging_items()


def test_a_release_policy_moves_to_feeds_visr_gg_with_its_pins():
    current = json.loads((ROOT / ".release/current.json").read_text())["build_policy"]
    moved = store.move_policy(current)
    assert {name: value for name, value in moved.items() if not name.endswith("_feed_url")} == \
        {name: value for name, value in current.items() if not name.endswith("_feed_url")}
    for name in ("profile_feed_url", "recording_feed_url", "shader_feed_url"):
        assert moved[name] == current[name].replace("https://code.steverice.org/visr/packs/", "https://feeds.visr.gg/")
    validation.load_policy(json.dumps(moved))
    validation.require_feed_host(moved)
    assert store.move_policy(moved) == moved
    with pytest.raises(ValueError, match="neither feed host"):
        store.move_policy({**current, "shader_feed_url": "https://example.invalid/tester/shaders.json"})


def test_publish_without_credentials_stops_before_any_upload(monkeypatch, capsys):
    for name in ("R2_ACCOUNT_ID", "R2_ACCESS_KEY_ID", "R2_SECRET_ACCESS_KEY"):
        monkeypatch.delenv(name, raising=False)
    with pytest.raises(ValueError, match="R2_ACCOUNT_ID"):
        store.main(["publish", "--backend", "s3"])


def test_builds_and_the_host_check_name_only_feeds_visr_gg(checked_in, tmp_path):
    from tools import feeds_host_check as check

    check.development_defaults()
    assert check.key_of("https://feeds.visr.gg/tester/device-profiles-4/ios/iPad11%2C3/_.json") == "tester/device-profiles-4/ios/iPad11,3/_.json"
    for url in ("https://code.steverice.org/visr/packs/tester/shaders.json", "http://feeds.visr.gg/tester/shaders.json"):
        with pytest.raises(ValueError):
            check.key_of(url)
    store.stage(tmp_path / "bucket", checked_in)
    try:
        profile, shaders, recording, compatibility = check.release_urls(tmp_path / "bucket")
    except check.Stale:
        pytest.skip("the pinned feeds tree predates this checkout's release policy")
    assert all(url.startswith("https://feeds.visr.gg/tester/") for url in (profile, shaders, recording))
    assert (tmp_path / "bucket" / check.key_of(recording)).is_file()
    assert re.fullmatch(r"benchmark-catalog-[12]:[0-9a-f]{64}", compatibility)


def test_the_host_check_stages_the_feeds_tree_as_it_is(tmp_path, feeds_tree):
    from tools import feeds_host_check as check

    check.copy_packs(feeds_tree, tmp_path / "bucket")
    receipt = json.loads((tmp_path / "bucket/tester/publication.json").read_bytes())["assets"]
    for path, digest in receipt.items():
        assert hashlib.sha256((tmp_path / "bucket" / store.key_for(path)).read_bytes()).hexdigest() == digest
    with pytest.raises(check.Stale):
        check.release_urls(tmp_path / "empty")
    with pytest.raises(ValueError, match="feeds tree"):
        check.copy_packs(tmp_path / "nothing", tmp_path / "other")


def test_record_mirrors_the_feeds_into_a_visr_feeds_checkout(checked_in, tmp_path):
    checkout = tmp_path / "visr-feeds"
    (checkout / ".git").mkdir(parents=True)
    (checkout / "README.md").write_bytes(b"kept")
    (checkout / ".github/workflows").mkdir(parents=True)
    (checkout / ".github/workflows/publish.yml").write_bytes(b"kept")
    (checkout / "tester/retired").mkdir(parents=True)
    (checkout / "tester/retired/old.json").write_bytes(b"gone")
    changed = store.record(checkout, checked_in)
    assert "tester/retired/old.json" in changed and not (checkout / "tester/retired").exists()
    assert (checkout / "README.md").read_bytes() == b"kept" and (checkout / ".github/workflows/publish.yml").exists()
    record = {path.relative_to(checkout).as_posix() for path in checkout.rglob("*")
              if path.is_file() and path.parts[len(checkout.parts)] in ("tester", "staging")}
    assert record == {entry.key for entry in checked_in} - {store.STAGING_RECEIPT}
    assert store.record(checkout, checked_in) == []
    shaders = checkout / "tester/shaders.json"
    shaders.write_bytes(b"edited")
    assert store.record(checkout, checked_in) == ["tester/shaders.json"]
    with pytest.raises(ValueError, match="checkout"):
        store.record(tmp_path / "not-a-checkout", checked_in)


def test_the_record_publishes_the_same_files_as_this_checkout(checked_in, tmp_path, host):
    checkout = tmp_path / "visr-feeds"
    (checkout / ".git").mkdir(parents=True)
    store.record(checkout, checked_in)
    from_tree = store.items_from(checkout)
    assert store.overlay.ROOT == ROOT
    assert {(entry.key, entry.sha256, entry.content_type, entry.cache_control) for entry in from_tree} == \
        {(entry.key, entry.sha256, entry.content_type, entry.cache_control) for entry in checked_in}
    served = host({entry.key: entry.data for entry in from_tree})
    assert store.verify(served.url, from_tree) == []
    with pytest.raises(ValueError, match="tester"):
        store.items_from(tmp_path / "empty")
    (checkout / "tester/shaders.json").write_bytes(b"not an envelope")
    with pytest.raises(ValueError):
        store.items_from(checkout)
    assert store.overlay.ROOT == ROOT


def test_the_record_workflow_pushes_the_record_and_visr_feeds_publishes_it():
    import yaml

    workflow = yaml.safe_load((ROOT / ".github/workflows/feeds-record.yml").read_text())
    triggers = workflow[True]
    assert triggers["workflow_run"]["workflows"] == ["Pages"] and "schedule" in triggers and "workflow_dispatch" in triggers
    job = workflow["jobs"]["record"]
    assert job["continue-on-error"] is True and job["environment"] == "github-pages"
    steps = job["steps"]
    assert "secrets.FEEDS_DEPLOY_KEY" in str(steps) and steps[2]["run"] == "tools/feeds_record.sh"
    assert steps[3]["if"] == "failure()" and "GITHUB_STEP_SUMMARY" in steps[3]["run"]
    assert "feeds" not in (ROOT / ".github/workflows/pages.yml").read_text().split("jobs:")[1]
    script = (ROOT / "tools/feeds_record.sh").read_text()
    assert "feeds_store.py record" in script and "push --quiet origin HEAD:refs/heads/main" in script
    assert "--force" not in script and "+HEAD" not in script
    publish = yaml.safe_load((ROOT / "tools/visr_feeds/publish.yml").read_text())
    job = publish["jobs"]["publish"]
    script = "\n".join(step.get("run", "") for step in job["steps"])
    assert job["environment"]["name"] == "r2-publish"
    assert "feeds_store.py publish --backend s3 --tree feeds" in script and "feeds_store.py verify --tree feeds" in script
    assert publish[True]["push"]["branches"] == ["main"]


def test_the_record_script_commits_once_and_never_forces(tmp_path):
    remote = tmp_path / "remote.git"
    # a default branch other than main, as on a host whose init.defaultBranch is master
    subprocess.run(["git", "init", "--quiet", "--bare", "--initial-branch=master", str(remote)], check=True)
    environment = {**os.environ, "FEEDS_DEPLOY_KEY": "unused for a local remote", "FEEDS_REMOTE": str(remote),
                   "FEEDS_ATTEMPTS": "2", "FEEDS_RETRY_SECONDS": "0", "FEEDS_PYTHON": sys.executable}
    for _ in range(2):
        run = subprocess.run([str(ROOT / "tools/feeds_record.sh")], cwd=ROOT, env=environment, capture_output=True, text=True)
        assert run.returncode == 0, run.stdout + run.stderr
    log = subprocess.run(["git", "--git-dir", str(remote), "log", "--format=%s", "main"], check=True,
                         capture_output=True, text=True).stdout.splitlines()
    assert len(log) == 1 and log[0].startswith("Feeds from steverice/visr@")
    missing = subprocess.run([str(ROOT / "tools/feeds_record.sh")], cwd=ROOT, capture_output=True, text=True,
                             env={**environment, "FEEDS_REMOTE": str(tmp_path / "missing.git")})
    assert missing.returncode == 1 and missing.stderr.count("could not reach visr-feeds") == 2



def envelope(compatibility: str, sequence: int, note: str = "") -> bytes:
    manifest = json.dumps({"compatibility": compatibility, "sequence": sequence, "note": note}).encode()
    return json.dumps({"manifest": base64.b64encode(manifest).decode(), "payload": "", "signature": ""}).encode()


def test_the_pinned_feeds_hold_every_release_exactly(feeds_tree):
    with store.tree_root(feeds_tree):
        assets = overlay.checked_in_assets()
        receipt = overlay.parse_receipt((feeds_tree / "tester/publication.json").read_bytes())
    profiles = [path for path in assets if "/profiles/" in path]
    recordings = [path for path in assets if "recordings" in Path(path).name]
    assert len(profiles) == 20
    assert not [path for path in assets if "/device-profiles-2/" in path]
    assert len([path for path in assets if "/device-profiles-3/" in path]) == 56
    assert len([path for path in assets if "/device-profiles-4/" in path]) == 56
    assert len(recordings) == 14
    assert set(assets) == set(receipt)
    assert hashlib.sha256(assets[RECORDING]).hexdigest() == (
        "8977e0e49588399d1c0f867d98f955b8a815b0c05e7463df2d1e92ae3ba64d89"
    )
    assert hashlib.sha256(assets[RECORDING_060]).hexdigest() == (
        "4ef803888532401d66d132d2963ac9ff8ef9cef017fd0769f576452b38b7f5b4"
    )
    assert hashlib.sha256(assets[RECORDING_061]).hexdigest() == (
        "b0f48ea1d1eeb77f8872d9fd0149e2e40e1d93913727a3dce4e000aa8b4d363d"
    )
    assert hashlib.sha256(assets[RECORDING_062]).hexdigest() == (
        "be6dddab12c382837eb40090e1cbaa3d614736cffef5de68ccf0981b526e516f"
    )
    assert hashlib.sha256(assets[RECORDING_063]).hexdigest() == (
        "e12d281950aa9d212e61fc8e934ddf8c32fdedfd0e1913918b3ecf8fc39cfc03"
    )
    assert hashlib.sha256(assets[RECORDING_065]).hexdigest() == (
        "549bd0661c3f370a89fa48e9e81b9b462c1692fffbe8ae29b38f98d5bf1cad79"
    )
    assert hashlib.sha256(assets[RECORDING_066]).hexdigest() == (
        "09d805fb76aed3d5c03e40e61bb5e186f28604aea8b5db9620c04e3ddb7ed027"
    )
    assert hashlib.sha256(assets[RECORDING_068]).hexdigest() == (
        "e56ffa8cb6758eb9ead55e8ec6bbee5bc74be21dc0947462d2c5d6c3f2dcd99d"
    )
    assert hashlib.sha256(assets[RECORDING_0610]).hexdigest() == (
        "e833453029389f5e4dd768dcb388f42f63a140a69b542793dd8846e8a76ebaeb"
    )
    assert hashlib.sha256(assets[RECORDING_0611]).hexdigest() == (
        "92274608883049fccd316c54367ad0e883688eb3bf141950c0e1246482d83a7b"
    )
    assert hashlib.sha256(assets[RECORDING_0612]).hexdigest() == (
        "b2ff82f8443e45e2b97e7c2be945885cd9a14e508ebaecf6e59ad2ce7df1363f"
    )
    assert hashlib.sha256(assets[RECORDING_069]).hexdigest() == (
        "fae46709054e8da3e6a9bc68ed0259ae0760236d5c21f47729cbc965faead291"
    )
    assert hashlib.sha256(assets[RECORDING_067]).hexdigest() == (
        "f9ada51d7285787a4615635274999a50647f635256a727f578eaf86274278ced"
    )
    envelope = json.loads(assets[SHADER])
    manifest = overlay.validation.packs.decode_json(
        overlay.validation.packs.decode_base64(envelope["manifest"], 65536)
    )
    payload = overlay.validation.packs.decode_json(
        overlay.validation.packs.decode_base64(envelope["payload"], 16 * 1024 * 1024)
    )
    assert manifest["sequence"] == 1793945199
    assert manifest["compatibility"] == "shader-lists-2"
    assert len(payload["files"]) == 24
    assert "b40.txt" in payload["files"]


def test_every_live_envelope_is_checked_even_when_no_receipt_names_it(checked_in, host, monkeypatch):
    shaders = next(entry for entry in checked_in if entry.key == "tester/shaders.json")
    # the bucket serves an envelope at the key but its receipt is missing (a partial earlier upload)
    served = host({"tester/shaders.json": b"older shader envelope"})
    seen = []

    def advance(path, old, new):
        seen.append(path)
        raise ValueError("Replacement asset needs a higher signed sequence")

    monkeypatch.setattr(store.overlay, "advance", advance)
    backend = store.DryRun()
    with pytest.raises(ValueError, match="higher signed sequence"):
        store.publish(backend, served.url, [shaders])
    assert seen == ["docs/site/packs/tester/shaders.json"] and backend.puts == []


@pytest.mark.parametrize("live_sequence, refused", [(5, True), (4, True), (3, False)])
def test_staging_feeds_never_go_back_in_sequence(host, live_sequence, refused):
    key = "staging/recordings-2-6e38e59159abea0d.json"
    new = store.Item(key, envelope("benchmark-catalog-2:" + "a" * 64, 4, "new"), *store.describe(key))
    served = host({key: envelope("benchmark-catalog-2:" + "a" * 64, live_sequence, "live")})
    backend = store.DryRun()
    if refused:
        with pytest.raises(ValueError, match="higher signed sequence"):
            store.publish(backend, served.url, [new])
    else:
        assert [entry.key for entry in store.publish(backend, served.url, [new])] == [key]
    other = host({key: envelope("benchmark-catalog-2:" + "b" * 64, 9, "other guest")})
    assert store.publish(store.DryRun(), other.url, [new]) == [new]
    garbage = host({key: b"not an envelope"})
    with pytest.raises(ValueError, match="Not a signed feed envelope"):
        store.publish(store.DryRun(), garbage.url, [new])
