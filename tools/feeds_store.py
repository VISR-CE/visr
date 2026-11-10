#!/usr/bin/env python3
"""Copy the signed feeds from their public record, VISR-CE/visr-feeds, to the R2 bucket behind
https://feeds.visr.gg/.

Builds read every feed from feeds.visr.gg (docs/data-packs.md, "Feed hosts"). The record is a visr-feeds checkout whose
tree is the bucket's layout: tester/... and staging/.... Feed writers (profile_pr.py, data_pack.py, latest_release.py)
write into it, it is merged to visr-feeds' main, and visr-feeds' own workflow runs `publish --tree` and `verify --tree`.

Bucket keys mirror the URL path: tester/shaders.json is
https://feeds.visr.gg/tester/shaders.json. Uploads go in an order that never lets a client read an index before the
files it names (content-addressed archives, profile shards, feed envelopes, the format marker, the receipt last), skip
what the bucket already serves, refuse to replace a live envelope with a lower signed sequence, and never delete.

  feeds_store.py publish --tree DIR --dry-run    what would be uploaded, and nothing else
  feeds_store.py publish --tree DIR --backend s3 CI: R2_ACCOUNT_ID, R2_ACCESS_KEY_ID, R2_SECRET_ACCESS_KEY
  feeds_store.py publish --tree DIR --backend wrangler   a local `wrangler login`
  feeds_store.py verify --tree DIR               every published file, byte for byte, with redirects refused
  feeds_store.py stage --tree DIR --output DIR   the bucket's tree as files, for a local stand-in server
  DIR is a VISR-CE/visr-feeds checkout (tester/, staging/)
  feeds_store.py policy --input policy.json      a release policy with its feed URLs moved to feeds.visr.gg
"""
from __future__ import annotations

import argparse
import base64
import contextlib
import datetime
import hashlib
import hmac
import json
import os
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request

if __package__:
    from . import pages_asset_overlay as overlay
    from .data_pack import ENVELOPE_LIMITS
else:
    import pages_asset_overlay as overlay
    from data_pack import ENVELOPE_LIMITS

FEEDS_URL = "https://feeds.visr.gg"
# where builds before 0.6.12 read the same files
PAGES_URL = "https://code.steverice.org/visr/packs"
BUCKET = "visr-feeds"
PACKS = "docs/site/packs/"
STAGING = PACKS + "staging/"
# staging/ has no Pages receipt; the bucket keeps its own beside the files, in the same shape
STAGING_RECEIPT = "staging/publication.json"
IMMUTABLE = "public, max-age=31536000, immutable"
MUTABLE = "no-cache"
MAX_STAGING_FILES = 64
# a compressed recording archive's bound (data_pack.RECORDING_ARCHIVE_MAX_COMPRESSED where catalog 2 exists)
MAX_STAGING_ARCHIVE = 32 * 1024 * 1024


@dataclass(frozen=True)
class Item:
    key: str
    data: bytes
    content_type: str
    cache_control: str
    rank: int

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.data).hexdigest()


def key_for(path: str) -> str:
    if not path.startswith(PACKS) or ".." in path.split("/"):
        raise ValueError("Feed files live under " + PACKS + ": " + path)
    return path.removeprefix(PACKS)


def describe(key: str) -> tuple[str, str, int]:
    """Content type, cache policy and upload rank for a bucket key."""
    name = key.rsplit("/", 1)[-1]
    if name.endswith((".zlib", ".xz")):
        return "application/octet-stream", IMMUTABLE, 0
    if name == "publication.json":
        return "application/json", MUTABLE, 4
    if name == "publication-format.txt":
        return "text/plain; charset=utf-8", MUTABLE, 3
    if name.endswith(".json"):
        directory = key.split("/")[1] if key.count("/") >= 2 else ""
        return "application/json", MUTABLE, 1 if directory in overlay.PROFILE_DIRECTORIES else 2
    raise ValueError("Unexpected feed file type: " + key)


def item(path: str, data: bytes) -> Item:
    key = key_for(path)
    return Item(key, data, *describe(key))


def tester_items() -> list[Item]:
    """The tester feeds exactly as Pages publishes them: each checked against its signature and the receipt."""
    assets = overlay.checked_in_assets()
    result = [item(path, data) for path, data in assets.items()]
    for path in (overlay.MARKER, overlay.RECEIPT):
        result.append(item(path, overlay.read_source(overlay.ROOT / path, overlay.MAX_RECEIPT)))
    return result


def staging_items() -> list[Item]:
    """Development recording feeds for unreleased guests: outside tester/, never read by a release. They get the same
    bounds as tester files and a receipt of their own."""
    directory = overlay.ROOT / STAGING
    if not directory.exists():
        return []
    files = {}
    for path in sorted(directory.rglob("*")):
        if path.is_symlink():
            raise ValueError("Staging feeds cannot be symbolic links")
        if path.is_dir():
            continue
        relative = path.relative_to(overlay.ROOT).as_posix()
        blob = re.fullmatch(re.escape(STAGING) + r"recording-blobs/([0-9a-f]{64})\.xz", relative)
        if blob:
            data = overlay.read_source(path, MAX_STAGING_ARCHIVE)
            if hashlib.sha256(data).hexdigest() != blob[1]:
                raise ValueError("Staging archive filename differs from its content hash: " + relative)
        elif re.fullmatch(re.escape(STAGING) + r"recordings(-[0-9a-z-]+)?\.json", relative):
            data = overlay.read_source(path, ENVELOPE_LIMITS["recordings"])
        else:
            raise ValueError("Unexpected staging feed file: " + relative)
        files[relative] = data
        if len(files) > MAX_STAGING_FILES:
            raise ValueError("Too many staging feed files")
    if not files:
        return []
    result = [item(path, data) for path, data in files.items()]
    result.append(Item(STAGING_RECEIPT, receipt_bytes({entry.key: entry.sha256 for entry in result}), *describe(STAGING_RECEIPT)))
    return result


def receipt_bytes(assets: dict[str, str]) -> bytes:
    return json.dumps({"schema": 1, "assets": dict(sorted(assets.items()))}, sort_keys=True, separators=(",", ":")).encode() + b"\n"


def feed_items() -> list[Item]:
    return tester_items() + staging_items()


@contextlib.contextmanager
def tree_root(tree: Path):
    """Read the feeds from a tree in the bucket's layout (a visr-feeds checkout): the validators read under their
    ROOT, so it points at a folder whose docs/site/packs is the tree."""
    if not (tree / "tester").is_dir() or (tree / "tester").is_symlink():
        raise ValueError(f"{tree} has no tester/ folder")
    saved = overlay.ROOT
    with tempfile.TemporaryDirectory(prefix="feeds-tree-") as temporary:
        (Path(temporary) / "docs/site").mkdir(parents=True)
        (Path(temporary) / "docs/site/packs").symlink_to(tree.resolve(), target_is_directory=True)
        overlay.ROOT = Path(temporary)
        try:
            yield
        finally:
            overlay.ROOT = saved


def items_from(tree: Path) -> list[Item]:
    with tree_root(tree):
        return feed_items()


def url_for(base: str, key: str) -> str:
    return base.rstrip("/") + "/" + urllib.parse.quote(key, safe="/,-_.~")


def require_base(base: str) -> None:
    parts = urllib.parse.urlsplit(base)
    local = parts.scheme == "http" and parts.hostname in {"127.0.0.1", "localhost"}
    if not (parts.scheme == "https" or local) or parts.query or parts.fragment or parts.username:
        raise ValueError("Feed hosts are read over HTTPS (plain HTTP only from this machine, for tests)")


def fetch(base: str, key: str, maximum: int) -> bytes | None:
    """The bytes served at key, or None for a 404. Redirects are refused, as the app and release validation do."""
    require_base(base)
    try:
        with urllib.request.build_opener(overlay.validation.NoRedirect()).open(url_for(base, key), timeout=45) as response:
            data = response.read(maximum + 1)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None
        raise ValueError(f"{url_for(base, key)}: HTTP {error.code}") from error
    if len(data) > maximum:
        raise ValueError("Served file exceeds its bounded size: " + key)
    return data


def live_receipts(base: str) -> dict[str, str]:
    """What the bucket says it serves: tester/ from the Pages-format receipt, staging/ from its own."""
    live: dict[str, str] = {}
    tester = fetch(base, key_for(overlay.RECEIPT), overlay.MAX_RECEIPT)
    if tester is not None:
        for path, digest in overlay.parse_receipt(tester).items():
            live[key_for(path)] = digest
        live[key_for(overlay.RECEIPT)] = hashlib.sha256(tester).hexdigest()
        marker = fetch(base, key_for(overlay.MARKER), len(overlay.MARKER_DATA))
        if marker is not None:
            live[key_for(overlay.MARKER)] = hashlib.sha256(marker).hexdigest()
    staging = fetch(base, STAGING_RECEIPT, overlay.MAX_RECEIPT)
    if staging is not None:
        value = json.loads(staging)
        if not isinstance(value, dict) or value.get("schema") != 1 or not isinstance(value.get("assets"), dict):
            raise ValueError("The bucket's staging receipt is invalid")
        for key, digest in value["assets"].items():
            if not isinstance(key, str) or not key.startswith("staging/") or not re.fullmatch(r"[0-9a-f]{64}", str(digest)):
                raise ValueError("The bucket's staging receipt names an invalid entry")
            live[key] = digest
        live[STAGING_RECEIPT] = hashlib.sha256(staging).hexdigest()
    return live


def plan(items: list[Item], live: dict[str, str]) -> list[Item]:
    """The uploads, in order: only what differs from the bucket's receipts, receipts last."""
    changed = [entry for entry in items if live.get(entry.key) != entry.sha256]
    return sorted(changed, key=lambda entry: (entry.rank, entry.key))


def manifest_order(data: bytes, key: str) -> tuple[str, int]:
    """An envelope's signed compatibility and sequence, read from its manifest (the signature is checked elsewhere)."""
    try:
        envelope = json.loads(data)
        manifest = json.loads(base64.b64decode(envelope["manifest"], validate=True))
        compatibility, sequence = manifest["compatibility"], manifest["sequence"]
    except (ValueError, KeyError, TypeError) as error:
        raise ValueError("Not a signed feed envelope: " + key) from error
    if not isinstance(compatibility, str) or type(sequence) is not int:
        raise ValueError("Not a signed feed envelope: " + key)
    return compatibility, sequence


def refuse_rollback(changes: list[Item], base: str) -> None:
    """Every envelope being uploaded is checked against the object the bucket serves at its key, whether or not a
    receipt names it: a tester envelope must advance under Pages' rule (a higher signed sequence, and for profile
    shards a revision no lower), and a staging feed must not replace the same compatibility with a sequence no higher."""
    for entry in changes:
        if entry.rank not in (1, 2):
            continue
        if entry.key.startswith("tester/"):
            path = PACKS + entry.key
            old = fetch(base, entry.key, overlay.limit_for(path))
            if old is not None:
                overlay.advance(path, old, entry.data)
            continue
        old = fetch(base, entry.key, ENVELOPE_LIMITS["recordings"])
        if old is None or old == entry.data:
            continue
        (old_domain, old_sequence), (domain, sequence) = manifest_order(old, entry.key), manifest_order(entry.data, entry.key)
        if domain == old_domain and sequence <= old_sequence:
            raise ValueError("Replacement asset needs a higher signed sequence: " + entry.key)


# --- uploads ---

def sigv4_headers(method: str, host: str, path: str, headers: dict[str, str], payload_sha256: str, *, access_key: str,
                  secret_key: str, region: str, service: str = "s3", now: datetime.datetime | None = None) -> dict[str, str]:
    """AWS Signature Version 4 for one request; path is already percent-encoded. R2's S3 API takes region auto."""
    moment = (now or datetime.datetime.now(datetime.timezone.utc)).astimezone(datetime.timezone.utc)
    amz_date, day = moment.strftime("%Y%m%dT%H%M%SZ"), moment.strftime("%Y%m%d")
    signed = {name.lower(): " ".join(str(value).split()) for name, value in headers.items()}
    signed.update({"host": host, "x-amz-content-sha256": payload_sha256, "x-amz-date": amz_date})
    names = sorted(signed)
    canonical = "\n".join([method, path, "", *(f"{name}:{signed[name]}" for name in names), "", ";".join(names), payload_sha256])
    scope = f"{day}/{region}/{service}/aws4_request"
    text = "\n".join(["AWS4-HMAC-SHA256", amz_date, scope, hashlib.sha256(canonical.encode()).hexdigest()])
    key = ("AWS4" + secret_key).encode()
    for part in (day, region, service, "aws4_request"):
        key = hmac.new(key, part.encode(), hashlib.sha256).digest()
    signature = hmac.new(key, text.encode(), hashlib.sha256).hexdigest()
    result = {name: value for name, value in signed.items() if name != "host"}
    result["authorization"] = f"AWS4-HMAC-SHA256 Credential={access_key}/{scope}, SignedHeaders={';'.join(names)}, Signature={signature}"
    return result


class DryRun:
    name = "dry run"

    def __init__(self) -> None:
        self.puts: list[Item] = []

    def put(self, entry: Item) -> None:
        self.puts.append(entry)


class S3:
    """R2's S3 API with an account API token scoped to the bucket (Object Read & Write)."""
    name = "R2 S3 API"

    def __init__(self, account_id: str, access_key: str, secret_key: str, bucket: str = BUCKET, endpoint: str = "") -> None:
        if not re.fullmatch(r"[0-9a-f]{32}", account_id) or not access_key or not secret_key:
            raise ValueError("R2 uploads need R2_ACCOUNT_ID (32 hex digits), R2_ACCESS_KEY_ID and R2_SECRET_ACCESS_KEY")
        self.endpoint = endpoint or f"https://{account_id}.r2.cloudflarestorage.com"
        self.bucket, self.access_key, self.secret_key = bucket, access_key, secret_key

    def put(self, entry: Item) -> None:
        path = "/" + urllib.parse.quote(self.bucket, safe="") + "/" + urllib.parse.quote(entry.key, safe="/-_.~")
        host = urllib.parse.urlsplit(self.endpoint).netloc
        headers = sigv4_headers("PUT", host, path, {"content-type": entry.content_type, "cache-control": entry.cache_control},
                                entry.sha256, access_key=self.access_key, secret_key=self.secret_key, region="auto")
        request = urllib.request.Request(self.endpoint + path, data=entry.data, method="PUT", headers=headers)
        try:
            with urllib.request.build_opener(overlay.validation.NoRedirect()).open(request, timeout=120) as response:
                response.read()
        except urllib.error.HTTPError as error:
            raise ValueError(f"Upload of {entry.key} failed: HTTP {error.code} {error.read(512)!r}") from error


class Wrangler:
    """A local `wrangler login`; wrangler itself comes from npx unless WRANGLER names a command."""
    name = "wrangler"

    def __init__(self, bucket: str = BUCKET, command: list[str] | None = None, scratch: Path | None = None, run=subprocess.run) -> None:
        self.bucket, self.run = bucket, run
        self.command = command or (os.environ["WRANGLER"].split() if os.environ.get("WRANGLER") else ["npx", "--yes", "wrangler@4.157.0"])
        self.scratch = scratch or overlay.ROOT / "build/feeds-store"

    def arguments(self, entry: Item, file: Path) -> list[str]:
        return [*self.command, "r2", "object", "put", f"{self.bucket}/{entry.key}", f"--file={file}",
                f"--content-type={entry.content_type}", f"--cache-control={entry.cache_control}", "--remote"]

    def put(self, entry: Item) -> None:
        file = self.scratch / entry.key
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(entry.data)
        try:
            self.run(self.arguments(entry, file), check=True)
        finally:
            file.unlink()


def publish(backend, base: str, items: list[Item]) -> list[Item]:
    live = live_receipts(base)
    changes = plan(items, live)
    refuse_rollback(changes, base)
    for entry in changes:
        backend.put(entry)
    return changes


def verify(base: str, items: list[Item]) -> list[str]:
    """Keys the host does not serve byte for byte."""
    return [entry.key for entry in items if fetch(base, entry.key, len(entry.data)) != entry.data]


def stage(output: Path, items: list[Item]) -> list[Item]:
    if output.exists():
        raise ValueError("Staging destination already exists")
    for entry in items:
        target = output / entry.key
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(entry.data)
    return items


def move_policy(policy: dict) -> dict:
    """A release policy whose feed URLs read from feeds.visr.gg. The bucket holds the same bytes as Pages, so the pinned
    SHA-256s stay as they are."""
    result = dict(policy)
    for name in sorted(name for name in policy if name.endswith("_feed_url")):
        url = policy[name]
        if isinstance(url, str) and url.startswith(PAGES_URL + "/"):
            result[name] = FEEDS_URL + "/" + url.removeprefix(PAGES_URL + "/")
        elif not (isinstance(url, str) and url.startswith(FEEDS_URL + "/")):
            raise ValueError(f"{name} is on neither feed host: {url!r}")
    return result


def summary(entries: list[Item]) -> str:
    return "\n".join(f"  {entry.key}  {len(entry.data)} bytes  {entry.content_type}  {entry.cache_control}" for entry in entries)


def main(arguments=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    publishing = commands.add_parser("publish", help="upload what the bucket does not serve yet")
    publishing.add_argument("--dry-run", action="store_true", help="print the uploads and make none")
    publishing.add_argument("--backend", choices=["s3", "wrangler"], default="s3")
    publishing.add_argument("--bucket", default=BUCKET)
    publishing.add_argument("--base-url", default=FEEDS_URL, help="where the bucket is served (default %(default)s)")
    checking = commands.add_parser("verify", help="check every published file byte for byte")
    checking.add_argument("--base-url", default=FEEDS_URL)
    staging = commands.add_parser("stage", help="write the bucket's tree to a folder")
    staging.add_argument("--output", type=Path, required=True)
    for command in (publishing, checking, staging):
        command.add_argument("--tree", type=Path, required=True, help="a VISR-CE/visr-feeds checkout (tester/, staging/)")
    policy = commands.add_parser("policy", help="move a release policy's feed URLs to feeds.visr.gg")
    policy.add_argument("--input", type=Path, help="policy JSON (default: standard input)")
    args = parser.parse_args(arguments)

    if args.command == "policy":
        text = args.input.read_text() if args.input else sys.stdin.read()
        print(json.dumps(move_policy(json.loads(text)), separators=(",", ":")))
        return 0
    items = items_from(args.tree)
    if args.command == "stage":
        entries = stage(args.output, items)
        print(f"Staged {len(entries)} files in {args.output}")
        return 0
    if args.command == "verify":
        missing = verify(args.base_url, items)
        for key in missing:
            print(f"differs or missing: {url_for(args.base_url, key)}", file=sys.stderr)
        print(f"{len(items) - len(missing)} of {len(items)} files served byte for byte at {args.base_url}")
        return 1 if missing else 0
    if args.dry_run:
        backend = DryRun()
    elif args.backend == "s3":
        backend = S3(os.environ.get("R2_ACCOUNT_ID", ""), os.environ.get("R2_ACCESS_KEY_ID", ""),
                     os.environ.get("R2_SECRET_ACCESS_KEY", ""), args.bucket)
    else:
        backend = Wrangler(args.bucket)
    changes = publish(backend, args.base_url, items)
    verb = "Would upload" if args.dry_run else "Uploaded"
    print(f"{verb} {len(changes)} files to {args.bucket} ({backend.name}), served at {args.base_url}")
    if changes:
        print(summary(changes))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, json.JSONDecodeError) as error:
        print(f"feeds_store: {error}", file=sys.stderr)
        sys.exit(1)
