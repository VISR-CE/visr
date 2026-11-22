"""Public repo hygiene: no private workspace paths, home folders or LAN addresses in tracked files."""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATTERNS = {
    "home folder": re.compile(r"/Users/(?!x/|private\b|you\b|name\b|<)[A-Za-z0-9_.-]+"),
    "private workspace": re.compile(r"code/halovision|halovision/scratch|step-tools|halovision-remote"),
    "LAN address": re.compile(r"\b192\.168\.\d+\.\d+"),
}
# Files that legitimately hold such text: vendored third-party code, tests that use fake values,
# this file, and the changelog (historic entries).
ALLOW_FILES = {"tools/test_no_private_paths.py", "CHANGELOG.md"}
ALLOW_PREFIXES = ("port/third_party/",)
# RFC 1918 range names and example addresses, not real hosts.
ALLOW_LINES = {("port/ios/tests/natpmp_probe.c", "192.168.1.20"), ("port/linux/src/p2p.c", "192.168.0.0")}


def tracked():
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, check=True).stdout
    return [p for p in out.decode().split("\0") if p]


def hits(kind):
    found = []
    for rel in tracked():
        if rel in ALLOW_FILES or rel.startswith(ALLOW_PREFIXES):
            continue
        try:
            text = (ROOT / rel).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for number, line in enumerate(text.splitlines(), 1):
            m = PATTERNS[kind].search(line)
            if m and (rel, m.group(0)) not in ALLOW_LINES:
                found.append(f"{rel}:{number}")
    return found


def test_no_home_folder_paths():
    assert hits("home folder") == []


def test_no_private_workspace_references():
    assert hits("private workspace") == []


def test_no_lan_addresses():
    assert hits("LAN address") == []
