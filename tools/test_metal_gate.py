"""The Metal GPU-family gate (`port/ios/host/metal_gate.h`) answers as the checks it replaced, and its feature rows
match Apple's table.

The bug class: a Metal feature with a GPU-family requirement used without a check, or checked against the wrong
family (known-bugs: non-uniform threadgroups on A8-A10X, MSAA resolve). The gate is the one place those requirements
live. `port/ios/tests/metal_gate_probe.c` resolves every GPU family from Apple2 to Apple11, a simulator with no family,
Apple silicon Macs and a Mac with no Apple family, and compares each answer with the scattered checks the gate replaced.
This test also compares every `METAL_FEATURES` row with `tools/data/metal-feature-families.tsv`, a copy of the edition
of Apple's table the rows name, and checks that no `supportsFamily:` check is written outside the gate.
"""
import csv
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "port/ios/host/metal_gate.h"
TABLE = ROOT / "tools/data/metal-feature-families.tsv"


def macro_rows(name):
    """The X(...) rows of the X-macro `name` in metal_gate.h, each as a list of its top-level arguments."""
    text = HEADER.read_text()
    match = re.search(rf"#define {name}\(X\)((?:[^\n]*\\\n)*[^\n]*)\n", text)
    assert match, name
    body = match.group(1).replace("\\\n", " ")
    rows = []
    for start in (found.end() for found in re.finditer(r"\bX\(", body)):
        depth, quoted, parts, part = 1, False, [], ""
        for character in body[start:]:
            if character == '"':
                quoted = not quoted
            elif not quoted and character == "(":
                depth += 1
            elif not quoted and character == ")":
                depth -= 1
                if not depth:
                    break
            if character == "," and depth == 1 and not quoted:
                parts.append(part.strip())
                part = ""
            else:
                part += character
        rows.append(parts + [part.strip()])
    return rows


def without_comments(source):
    """source with its C comments blanked, line numbers kept."""
    blank = lambda match: re.sub(r"[^\n]", " ", match.group(0))
    return re.sub(r"//[^\n]*", blank, re.sub(r"/\*[\s\S]*?\*/", blank, source))


def apple_table():
    with TABLE.open() as handle:
        lines = [line for line in handle if not line.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


CAPABILITY_CHECK = re.compile(r"supportsFamily[:(]|supportsBCTextureCompression|supports32BitFloatFiltering|"
                              r"supportsRasterizationRateMapWithLayerCount:|supportsCounterSampling:|"
                              r"MTLFXSpatialScalerDescriptor supportsDevice:|supportsRaytracing")


def raw_capability_checks(source):
    """'line: text' for each line of source, outside comments, that asks the device a capability directly."""
    return [f"{number}: {line.strip()}" for number, line in enumerate(without_comments(source).splitlines(), 1)
            if CAPABILITY_CHECK.search(line)]


class MetalGateTests(unittest.TestCase):
    def test_every_family_answers_as_before(self):
        compiler = shutil.which("clang") or shutil.which("cc")
        if not compiler:
            self.skipTest("A C compiler is required")
        with tempfile.TemporaryDirectory(prefix="metal-gate-probe-") as temp:
            executable = pathlib.Path(temp) / "probe"
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O1",
                "-I", str(ROOT / "port/ios/host"), str(ROOT / "port/ios/tests/metal_gate_probe.c"),
                "--output", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)
            blocks = subprocess.run([str(executable), "blocks"], check=True, capture_output=True, text=True).stdout
        # the fleet's oldest GPUs: the A8 has no pipeline archive or base vertex draws; the A10X has them
        lines = blocks.splitlines()
        # each GPU's name, then its two log lines
        a8 = " ".join(lines[lines.index("Apple TV HD (A8)") + 1:][:2])
        a10x = " ".join(lines[lines.index("iPad Pro A10X") + 1:][:2])
        self.assertIn("Metal gate: Apple2 (A8).", a8)
        self.assertIn("BINARY_ARCHIVES", a8)
        self.assertIn("Metal gate: Apple3 (A9-A10).", a10x)
        self.assertNotIn("BINARY_ARCHIVES", a10x)
        self.assertIn("ambient occlusion off (needs SIMD_PERMUTES, Apple6)", a10x)
        # the A8 and the A10X fault their GPUs on the soft particle compute dispatch: Apple4 (A11) and later run it
        self.assertIn("soft particles off (A8 and A10X compute dispatch faults)", a8)
        self.assertIn("soft particles off (A8 and A10X compute dispatch faults)", a10x)

    def test_feature_rows_match_apples_table(self):
        documented = {}
        for row in apple_table():
            if row["gate_feature"]:
                documented[row["gate_feature"]] = int(row["first_apple_family"])
        edition = re.search(r"#define METAL_GATE_EDITION (\d+)", HEADER.read_text()).group(1)
        rows = macro_rows("METAL_FEATURES")
        self.assertTrue(rows)
        for name, family, mac, query, row_edition in rows:
            with self.subTest(feature=name):
                self.assertIn(name, documented, "a gate feature with no row in Apple's table copy")
                self.assertEqual(int(family), documented[name])
                self.assertEqual(row_edition, edition)
        self.assertEqual(set(documented), {row[0] for row in rows}, "table rows naming a gate feature it lacks")

    def test_table_copy_is_whole(self):
        rows = apple_table()
        names = {row["feature"] for row in rows}
        # a few rows from each page of the edition, so a truncated copy fails
        for name in ("MetalKit", "Tessellation", "SIMD-scoped permute operations", "Relaxed math",
                     "FP32 denormalized numbers", "Sampler LOD bias"):
            self.assertIn(name, names)
        self.assertGreaterEqual(len(rows), 150)
        for row in rows:
            self.assertTrue(2 <= int(row["first_apple_family"]) <= 11, row)

    def test_pass_floors_cover_their_features(self):
        families = {name: int(family) for name, family, *_ in macro_rows("METAL_FEATURES")}
        for row in macro_rows("METAL_PASSES"):
            name, needs, floor = row[0], row[1], int(row[3])
            for feature in re.findall(r"METAL_NEEDS\((\w+)\)", needs):
                with self.subTest(pass_=name, feature=feature):
                    self.assertGreaterEqual(floor, families[feature])

    def test_enforced_passes_are_read_and_others_are_not(self):
        # a pass marked enforced must be read by the backend, and one marked "[not enforced yet]" must not be, so the log
        # never says a wired pass is unenforced or the other way around
        read = set()
        for folder in ("port/ios/host", "port/macos"):
            if not (ROOT / folder).is_dir():
                continue
            for path in (ROOT / folder).rglob("*"):
                if path.suffix in {".m", ".mm", ".c", ".swift"}:
                    read |= set(re.findall(r"\bmetal_pass\(\s*METAL_PASS_(\w+)\s*\)",
                                           without_comments(path.read_text(errors="replace"))))
        for row in macro_rows("METAL_PASSES"):
            name, enforced = row[0], row[5] == "1"
            with self.subTest(pass_=name):
                self.assertEqual(name in read, enforced,
                                 "set the row's enforced flag to match whether metal_pass() reads it")

    def test_capability_checks_go_through_the_gate(self):
        # the device's own answers are asked in one file; everything else reads the gate
        calls = []
        for folder in ("port/ios/host", "port/macos"):
            if not (ROOT / folder).is_dir():
                continue
            for path in sorted((ROOT / folder).rglob("*")):
                if path.suffix not in {".m", ".mm", ".c", ".h", ".swift"} or path.name == "metal_gate.m":
                    continue
                calls += [f"{path.relative_to(ROOT)}:{line}" for line in raw_capability_checks(path.read_text(errors="replace"))]
        self.assertFalse(calls, "ask metal_feature() or metal_pass() (metal_gate.h) instead:\n" + "\n".join(calls))

    def test_the_scan_finds_a_raw_check(self):
        # negative control: each form a hand-written check took before the gate is found, and a comment is not
        for line in ("ok = [device supportsFamily:MTLGPUFamilyApple5] || x;",
                     "if (@available(iOS 16.4, *)) bc = device.supportsBCTextureCompression;",
                     "if (![device supportsRasterizationRateMapWithLayerCount:1])",
                     "mode = [device supportsCounterSampling:MTLCounterSamplingPointAtDrawBoundary];",
                     "if (![MTLFXSpatialScalerDescriptor supportsDevice:device] ||",
                     "capable = [device supportsRaytracing] && [device supportsRaytracingFromRender];",
                     "let ok = device.supportsFamily(.apple7)"):
            with self.subTest(line=line):
                self.assertEqual(len(raw_capability_checks("int a;\n" + line + "\n")), 1)
        self.assertFalse(raw_capability_checks("/* supportsFamily: Apple3 */\n// supportsBCTextureCompression\n"))

if __name__ == "__main__":
    unittest.main()
