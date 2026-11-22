"""Public repo hygiene: tracked files must not hold private workspace paths, home folders, private network
addresses, device identifiers, or (when a private list is available) device nicknames and host names.

Set VISR_PRIVATE_TERMS to a file with one case-insensitive term per line (device nicknames, host names, account
names) to add those to the scan; the list stays outside the repository, and the check is skipped when it is absent."""
import os
import re
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
_OCTET = r"(?:25[0-5]|2[0-4]\d|1?\d?\d)"
PATTERNS = {
    "home folder": re.compile(r"/Users/[A-Za-z0-9_.-]+|/home/[A-Za-z][A-Za-z0-9_.-]*"),
    "mounted volume": re.compile(r"/Volumes/"),
    "private workspace": re.compile(r"halovision|step[-_ ]?tools|agent-(?:specs|plans)", re.IGNORECASE),
    # Not followed by "/<bits>": a bare range such as 100.64.0.0/10 is documentation, an address is a host.
    "private address": re.compile(
        rf"(?<![\d.])(?:10\.{_OCTET}|172\.(?:1[6-9]|2\d|3[01])|192\.168|100\.(?:6[4-9]|[7-9]\d|1[01]\d|12[0-7]))"
        rf"\.{_OCTET}\.{_OCTET}(?![\d.]|/\d)"),
    "device identifier": re.compile(r"\b[0-9A-Fa-f]{8}-[0-9A-Fa-f]{16}\b"),
}
# Files that are not ours.
ALLOW_PREFIXES = ("port/third_party/",)
# (file, matched text) pairs that are fake values or documentation examples, not real hosts or folders.
ALLOW_MATCHES = {
    ("tools/test_mac_run.py", "/Users/x"),
    ("tools/test_perf_compare_refresh.py", "/Users/private"),
    ("port/ios/tests/natpmp_probe.c", "192.168.1.20"),
    ("port/ios/tests/lan_targets_probe.c", "10.0.0.2"),
    ("port/ios/tests/lan_targets_probe.c", "10.0.0.3"),
    ("port/ios/tests/lan_targets_probe.c", "10.0.0.4"),
    ("port/ios/tests/lan_targets_probe.c", "10.0.0.255"),
}
SELF = "tools/test_no_private_paths.py"


def private_terms():
    path = os.environ.get("VISR_PRIVATE_TERMS", "")
    if not path or not Path(path).is_file():
        return None
    words = [line.strip() for line in Path(path).read_text(encoding="utf-8").splitlines()
             if line.strip() and not line.lstrip().startswith("#")]
    if not words:
        return None
    return re.compile(r"(?<![A-Za-z0-9])(?:" + "|".join(re.escape(w) for w in words) + r")(?![A-Za-z0-9])", re.IGNORECASE)


def scan_text(rel, text, patterns):
    """Every match of every pattern on every line, as 'file:line: kind: text'."""
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        for kind, pattern in patterns.items():
            for m in pattern.finditer(line):
                if (rel, m.group(0)) not in ALLOW_MATCHES:
                    found.append(f"{rel}:{number}: {kind}: {m.group(0)}")
    return found


def tracked():
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, check=True).stdout
    return [p for p in out.decode().split("\0") if p and p != SELF and not p.startswith(ALLOW_PREFIXES)]


def scan_tree(patterns):
    found = []
    for rel in tracked():
        try:
            text = (ROOT / rel).read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue  # binary
        except OSError:
            continue  # listed but absent
        found += scan_text(rel, text, patterns)
    return found


def test_tree_has_no_private_text():
    assert scan_tree(PATTERNS) == []


def test_tree_has_no_private_terms():
    terms = private_terms()
    if terms is None:
        pytest.skip("VISR_PRIVATE_TERMS is not set to a readable list")
    assert scan_tree({"private term": terms}) == []


@pytest.mark.parametrize("sample", [
    "/Users/someone/code", "/home/someone/code", "/Volumes/scratch/maps", "~/code/halovision/x", "Step-Tools/run.sh",
    "step_tools", "~/code/agent-specs/a.md", "10.1.2.3", "172.20.0.5", "100.100.1.1", "192.168.1.9",
    "00008160-001271C81AA80036",
])
def test_patterns_catch(sample):
    assert scan_text("x.txt", sample, PATTERNS)


@pytest.mark.parametrize("sample", ["100.64.0.0/10", "10.0.0.0/8", "version 1.10.0.2", "/Users/", "172.32.0.1"])
def test_patterns_pass(sample):
    assert scan_text("x.txt", sample, PATTERNS) == []


def test_every_match_on_a_line_is_reported():
    line = "192.168.1.20 then /Users/someone/a and 10.9.9.9"
    assert len(scan_text("port/ios/tests/natpmp_probe.c", line, PATTERNS)) == 2


def test_terms_match_whole_words_without_case(tmp_path, monkeypatch):
    list_file = tmp_path / "terms"
    list_file.write_text("# comment\nSome Nick\n", encoding="utf-8")
    monkeypatch.setenv("VISR_PRIVATE_TERMS", str(list_file))
    terms = private_terms()
    assert scan_text("x.txt", "the some nick iPad", {"t": terms})
    assert not scan_text("x.txt", "somenicks", {"t": terms})
