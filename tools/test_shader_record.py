"""The shader record contract (port/runtime/shader_record.h) and its tool (shader-record)

Bug classes this covers: two definitions of one layout drift apart (the struct against the table); the tool step
validates less than the gate after it (merge against check); a diagnosis from a mislabeled byte (explain); records
that depend on state outside the file (close); a reader refuses what a writer produced (round trip, reduce).
"""
import json
import random
import re
import shutil
import struct
import subprocess
from pathlib import Path

import pytest

import data_pack
from tools import shader_lists

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tools/fixtures/shader-records"
TOOL = str(data_pack.shader_record_tool())


def fnv(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f"{value:016x}"


def key(count=0, **fields):
    """a 252-byte canonical key; `fields` set bytes by name (sampler_type=[...], fog_enable=1, ...)"""
    words = [0] * 57
    words[53] = count
    for base in (0, 26, 34, 45):
        for stage in range(count):
            words[base + stage] = 0x11 + stage
    body = bytearray(struct.pack("<57I", *words) + bytes(24))
    for name, value in fields.items():
        offset, size, element = ROWS[name]
        values = value if isinstance(value, list) else [value]
        for index, one in enumerate(values):
            body[offset + (index * size): offset + (index * size) + size] = one.to_bytes(size, "little")
    return bytes(body)


def ps(body):
    return f"ps {fnv(body)} {body.hex()}"


def run(*arguments, check=False):
    return subprocess.run([TOOL, *arguments], capture_output=True, text=True, check=check)


def table():
    """the key's rows as the tool prints them: {name: (offset, size, count, rule, minimum, maximum)}"""
    rows = {}
    for line in run("fields", check=True).stdout.splitlines():
        parts = line.split("\t")
        if parts[0] != "pipeline":
            offset, size, count, rule, minimum, maximum, name = parts
            rows[name] = (int(offset), int(size), int(count), rule, int(minimum), int(maximum))
    return rows


TABLE = table()
ROWS = {name: (row[0], row[1], row[2]) for name, row in TABLE.items()}
VS = "vs 00000000000000aa 0 0"


def pipeline(pixel, vertex="00000000000000aa.0.0", fields="0 0 0 0 15 0 1", kinds="00" * 16, alpha="0 0"):
    return f"pipeline {vertex} {pixel} {fields} {kinds} {alpha}".rstrip()


def folder_with(tmp_path, text, name="a10.txt", sub="in"):
    folder = tmp_path / sub
    folder.mkdir(parents=True, exist_ok=True)
    (folder / name).write_text(text)
    return folder


# ---- the app's admission did not change ----

@pytest.fixture(scope="module")
def probes(tmp_path_factory):
    """the validator as the app had it before the contract, and as it is now, both as `probe FOLDER` commands"""
    build = tmp_path_factory.mktemp("probes")
    flags = ["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-D_DARWIN_C_SOURCE=1", "-O1", "-w", f"-I{ROOT / 'port/ios/host'}"]
    legacy = build / "legacy"
    subprocess.run([*flags, str(ROOT / "port/ios/tests/shader_pack_probe.c"),
                    str(FIXTURES / "legacy/host_shader_pack_legacy.c"), "-o", str(legacy)], check=True)
    return legacy, data_pack.shader_pack_probe()


def corpus():
    """records that set each field of each kind to its minimum, its maximum and one past each, and malformed forms"""
    cases = []
    base = key(2, fog_enable=1)

    def mutate(**fields):
        return key(2, **{"fog_enable": 1, **fields})

    for name, (offset, size, count, rule, minimum, maximum) in TABLE.items():
        for index in range(count if size == 1 else 1):
            if rule == "range":
                values = [minimum, maximum, maximum + 1, 255]
            elif rule == "compare":
                values = [0, 1, 0x1ff, 0x200, 0x207, 0x208]
            elif rule in ("free", "texmodes"):
                values = [0xffffffff] if size == 4 else [0]
            else:
                continue
            for value in values:
                if name == "combiner_state":
                    body = bytearray(base)
                    body[0:4] = value.to_bytes(4, "little")
                    cases.append((f"{name}={value}", ps(bytes(body))))
                elif size == 1 and count > 1:
                    cases.append((f"{name}[{index}]={value}", ps(mutate(**{name: [0] * index + [value] + [0] * (count - index - 1)}))))
                else:
                    cases.append((f"{name}={value}", ps(mutate(**{name: value}))))
    for size in (251, 253, 256, 264):
        cases.append((f"size {size}", ps((base + bytes(20))[:size] if size > 252 else base[:size])))
    good = ps(base).split()
    cases += [("wrong hash", f"ps {'1' * 16} {good[2]}"), ("upper hash", f"ps {good[1].upper()} {good[2]}"),
              ("upper key", f"ps {good[1]} {good[2].upper()}"), ("odd key", f"ps {good[1]} {good[2][:-1]}"),
              ("short hash", f"ps {good[1][:15]} {good[2]}"), ("extra field", f"{ps(base)} x"), ("tabs", ps(base).replace(" ", "\t")),
              ("ps alone", "ps")]
    for variant in ("0", "1", "2", "3", "01", "+1", "", "-1", "1x", "4294967296", "99999999999"):
        for mask in ("0", "e", "ffff", "10000", "FFFF", "0x1", "", "00e", "fffff", "ffffffffff"):
            cases.append((f"vs {variant} {mask}", f"vs 00000000000000aa {variant} {mask}".rstrip()))
    cases += [("vs bad hash", "vs 00000000000000a 0 0"), ("vs extra", "vs 00000000000000aa 0 0 0"), ("vs upper", "vs 00000000000000AA 0 0")]
    maxima = {"blend": 1, "source": 14, "destination": 14, "operation": 4, "write_mask": 15, "exact_borders": 15, "depth": 1}
    names = list(maxima)
    for index, name in enumerate(names):
        for value in (0, maxima[name], maxima[name] + 1, -1):
            fields = [0, 0, 0, 0, 15, 0, 1]
            fields[index] = value
            cases.append((f"pipeline {name}={value}", pipeline("{ps}", fields=" ".join(map(str, fields)))))
    for alpha in ("0 0", "14 14", "15 0", "0 15", "x 0"):
        cases.append((f"pipeline alpha {alpha}", pipeline("{ps}", alpha=alpha)))
    cases.append(("pipeline 11 fields", pipeline("{ps}", alpha="")))
    cases.append(("pipeline 12 fields", pipeline("{ps}", alpha="0")))
    cases.append(("pipeline 14 fields", pipeline("{ps}", alpha="0 0 0")))
    for position in range(16):
        for value in ("13", "14", "00"):
            kinds = ["00"] * 16
            kinds[position] = value
            cases.append((f"pipeline kind {position}={value}", pipeline("{ps}", kinds="".join(kinds))))
    cases.append(("pipeline short kinds", pipeline("{ps}", kinds="00" * 15)))
    for vertex in ("00000000000000aa.0.0", "00000000000000aa.1.0", "00000000000000aa.1.e", "00000000000000aa.2.0",
                   "00000000000000aa.00.0", "00000000000000aa..0", "00000000000000aa.0.", "00000000000000aa.0.0.0",
                   "00000000000000a.0.0", "00000000000000aa.0.FF", "00000000000000aa.0.10000"):
        cases.append((f"pipeline vertex {vertex}", pipeline("{ps}", vertex=vertex)))
    return cases


def wrap(line, pixel_line):
    """the file holding a case: a pipeline gets the vs and ps it names, whose own validity is not under test"""
    if line.startswith("pipeline"):
        pixel_hash = pixel_line.split()[1]
        text = line.replace("{ps}", pixel_hash)
        vertex = text.split()[1].split(".")
        deps = [f"vs {vertex[0]} {vertex[1]} {vertex[2]}", pixel_line]
        return "\n".join(deps + [text]) + "\n"
    return line + "\n"


def test_the_contract_admits_and_refuses_exactly_what_the_legacy_validator_did(tmp_path, probes):
    legacy, current = probes
    pixel_line = ps(key(2, fog_enable=1))
    disagreements, refused, accepted = [], 0, 0
    for label, line in corpus():
        folder = folder_with(tmp_path, wrap(line, pixel_line), sub="case")
        verdicts = [subprocess.run([str(probe), str(folder)], capture_output=True).returncode == 0 for probe in (legacy, current)]
        if verdicts[0] != verdicts[1]:
            disagreements.append((label, verdicts))
        accepted += verdicts[0]
        refused += not verdicts[0]
        shutil.rmtree(folder)
    assert not disagreements
    assert accepted > 40 and refused > 150  # the corpus bites in both directions


def real_files():
    files = sorted((ROOT / "port/shader-lists").glob("*.txt"))
    files += sorted(path for path in FIXTURES.rglob("*.txt"))
    return files


def test_the_contract_agrees_with_the_legacy_validator_on_every_real_file(tmp_path, probes):
    legacy, current = probes
    for path in real_files():
        folder = folder_with(tmp_path, path.read_text(), name="m.txt", sub="real")
        verdicts = [subprocess.run([str(probe), str(folder)], capture_output=True).returncode == 0 for probe in (legacy, current)]
        assert verdicts[0] == verdicts[1], path
        shutil.rmtree(folder)


def test_the_bundled_lists_pass_admission_and_lint():
    assert run("check", "--lint", str(ROOT / "port/shader-lists")).returncode == 0


# ---- refusals name the field ----

def test_a_sampler_type_of_six_names_its_field_and_range(tmp_path):
    body = key(2, sampler_type=[6, 0, 0, 0])
    folder = folder_with(tmp_path, f"{ps(body)}\n")
    result = run("check", str(folder))
    assert result.returncode == 1
    assert "a10.txt:1: sampler_type[0]=6, allowed 0-5" in result.stderr


@pytest.mark.parametrize("name", [name for name, row in TABLE.items() if row[3] == "range"])
def test_every_range_row_passes_at_its_maximum_and_fails_one_past_it_by_name(tmp_path, name):
    offset, size, count, _, minimum, maximum = TABLE[name]
    for index in range(count):
        for value, passes in ((maximum, True), (maximum + 1, False)):
            body = bytearray(key(2))
            body[offset + index * size] = value
            folder = folder_with(tmp_path, f"{ps(bytes(body))}\n", sub=f"{name}{index}{value}")
            result = run("check", str(folder))
            label = f"{name}[{index}]" if count > 1 else name
            assert (result.returncode == 0) == passes, (label, value, result.stderr)
            if not passes:
                assert f"{label}={value}, allowed {minimum}-{maximum}" in result.stderr


def test_the_comparison_word_names_its_field(tmp_path):
    body = bytearray(key(2))
    body[244:248] = (1).to_bytes(4, "little")
    result = run("check", str(folder_with(tmp_path, ps(bytes(body)) + "\n")))
    assert "alpha_test_function=0x1, allowed 0 or 0x200-0x207" in result.stderr


def test_every_problem_is_reported_not_only_the_first(tmp_path):
    bad = key(2, sampler_type=[6, 0, 0, 0])
    text = "\n".join([ps(bad), "vs 00000000000000aa 2 0", pipeline("1" * 16), ""])
    result = run("check", str(folder_with(tmp_path, text)))
    assert "a10.txt:1: sampler_type[0]=6" in result.stderr
    assert "a10.txt:2: variant=2, allowed 0-1" in result.stderr
    assert "a10.txt:3: missing dependency ps 1111111111111111" in result.stderr


def test_a_lit_key_is_refused_with_the_way_out(tmp_path):
    lit = key(2) + struct.pack("<4B2I", 0, 0, 0, 0, 7, 9)
    result = run("check", str(folder_with(tmp_path, f"ps {fnv(lit)} {lit.hex()}\n")))
    assert "key_size=264, allowed 252; an extended or lit key: reduce it to its base" in result.stderr


def test_lint_refuses_what_only_tools_refuse(tmp_path):
    words = [0] * 57
    words[53] = 2
    words[5] = 99  # a stage past the combiner count holds a value: not canonical, though the app admits it
    body = struct.pack("<57I", *words) + bytes(24)
    folder = folder_with(tmp_path, ps(body) + "\n")
    assert run("check", str(folder)).returncode == 0
    refused = run("check", "--lint", str(folder))
    assert refused.returncode == 1 and "combiner_state: registers past the combiner count" in refused.stderr
    modes = bytearray(key(2))
    modes[228:232] = (19).to_bytes(4, "little")
    assert "texture_modes[0]=19, allowed 0-18" in run("check", "--lint", str(folder_with(tmp_path, ps(bytes(modes)) + "\n", sub="modes"))).stderr


# ---- explain decodes by the table's names ----

def test_explain_names_the_bytes_the_misreadings_confused(tmp_path):
    """the Apple TV report TKVZA9F8: byte 248 is fog_enable (read once as a comparison), 232-235 are sampler types
    (read once as texture modes), and the comparison word at 244 is 0"""
    out = run("explain", str(FIXTURES / "TKVZA9F8/intervals/TKVZA9F8-01/a50.txt"), "2350479156b6af5d", check=True).stdout
    assert re.search(r"alpha_test_function\s+=0 \(0x0\)\s+bytes 244-247", out)
    assert re.search(r"fog_enable\s+=1\s+byte 248", out)
    assert re.search(r"sampler_type\s+\[0\]=1 \[1\]=5 \[2\]=5 \[3\]=5\s+bytes 232-235", out)
    assert re.search(r"texture_modes\s+=33825 \(0x8421\)", out)
    assert "admitted" in out


def test_explain_a_line_decodes_each_kind():
    assert "variant=1" in run("explain", "vs 00000000000000aa 1 0", check=True).stdout
    out = run("explain", pipeline("1" * 16, fields="1 5 0 0 7 0 0"), check=True).stdout
    assert "blend=1 " in out and "source=5 " in out


# ---- close ---------------------------------------------------------------

def read_entries(path):
    return [line for line in path.read_text().splitlines() if line and not line.startswith("#")]


def close(tmp_path, delta, bases=(), also=(), sub="out"):
    out = tmp_path / sub
    command = ["close", "--out", str(out), "--report", str(tmp_path / f"{sub}.json")]
    for base in bases:
        command += ["--base", str(base)]
    for extra in also:
        command += ["--also", str(extra)]
    result = run(*command, str(delta))
    assert result.returncode == 0, result.stderr
    return out, json.loads((tmp_path / f"{sub}.json").read_text())


def test_close_synthesizes_a_missing_vertex_shader_and_finds_a_pixel_shader_in_a_base(tmp_path):
    body = key(2, fog_enable=1)
    base = folder_with(tmp_path, f"{VS}\n{ps(body)}\n", sub="base")
    delta = folder_with(tmp_path, pipeline(fnv(body), vertex="1234567890abcdef.1.0") + "\n")
    out, report = close(tmp_path, delta, bases=[base])
    assert read_entries(out / "a10.txt") == ["vs 1234567890abcdef 1 0", ps(body),
                                              pipeline(fnv(body), vertex="1234567890abcdef.1.0")]
    assert report["unresolved_pipelines"] == 0
    assert run("check", "--lint", str(out)).returncode == 0


def test_close_finds_a_pixel_shader_in_another_maps_file_and_lists_what_it_cannot_find(tmp_path):
    body = key(3)
    other = folder_with(tmp_path, f"{ps(body)}\n", name="b30.txt", sub="cand")
    delta = folder_with(tmp_path, "\n".join([pipeline(fnv(body)), pipeline("f" * 16, vertex="00000000000000bb.0.0")]) + "\n")
    out, report = close(tmp_path, delta, also=[other])
    assert report["maps"]["a10"]["unresolved"] == [{"hash": "f" * 16, "pipelines": 1, "source": str(delta)}]
    assert [line.split()[0] for line in read_entries(out / "a10.txt")] == ["vs", "ps", "pipeline"]
    assert run("check", "--lint", str(out)).returncode == 0


def test_close_reduces_lit_records_to_their_base_and_a_pipeline_follows(tmp_path):
    base = key(2, fog_enable=1)
    lit = base + struct.pack("<4B2I", 0, 0, 3, 2, 0x1234, 0x5678)
    text = "\n".join([f"vs 00000000000000aa 2 e", f"ps {fnv(lit)} {lit.hex()}",
                      pipeline(fnv(lit), vertex="00000000000000aa.2.e"), ""])
    out, _ = close(tmp_path, folder_with(tmp_path, text))
    assert read_entries(out / "a10.txt") == ["vs 00000000000000aa 0 e", ps(base), pipeline(fnv(base), vertex="00000000000000aa.0.e")]
    assert run("check", "--lint", str(out)).returncode == 0


def test_close_looks_a_pixel_shader_up_by_its_recorded_hash_as_well_as_its_canonical_one(tmp_path):
    dirty = bytearray(key(2))
    dirty[4 * 5: 4 * 5 + 4] = (0xdead).to_bytes(4, "little")  # stage 5's register: past the count of 2
    clean = key(2)
    other = folder_with(tmp_path, f"{ps(bytes(dirty))}\n", name="b30.txt", sub="cand")
    delta = folder_with(tmp_path, pipeline(fnv(bytes(dirty))) + "\n")
    out, _ = close(tmp_path, delta, also=[other])
    assert pipeline(fnv(clean)) in read_entries(out / "a10.txt")


def test_close_of_a_closed_file_changes_nothing(tmp_path):
    for path in sorted((ROOT / "port/shader-lists").glob("*.txt"))[:3]:
        folder = folder_with(tmp_path, path.read_text(), name=path.name, sub=f"in-{path.stem}")
        out, report = close(tmp_path, folder, sub=f"out-{path.stem}")
        assert (out / path.name).read_text() == path.read_text()
        assert report["unresolved_pipelines"] == 0


REPORTS = ("5KWC86NS", "TKVZA9F8", "XYYABR5J", "Y5LF8AUP")


def intervals():
    return [path for code in REPORTS for path in sorted((FIXTURES / code / "intervals").iterdir())]


# pipelines read, distinct pipelines in the closed files (a canonical key folds recorded duplicates into one), and
# pipelines left out because their pixel shader is in no candidate file
PINNED_TOTALS = {"in": 84, "closed": 39, "unresolved": 13}


def test_close_pins_what_it_does_to_the_real_interval_files(tmp_path):
    """twenty intervals from four reports, closed against only the report's own dependency candidates; the
    totals are pinned so a change in how `close` resolves is a visible diff"""
    totals = {"in": 0, "closed": 0, "unresolved": 0}
    for index, interval in enumerate(intervals()):
        code = interval.parent.parent.name
        out, report = close(tmp_path, interval, also=[FIXTURES / code / "dependency-candidates"], sub=f"o{index}")
        for stats in report["maps"].values():
            totals["in"] += stats["pipelines_in"]
            totals["closed"] += stats["pipelines_closed"]
            totals["unresolved"] += sum(item["pipelines"] for item in stats["unresolved"])
        if out.exists():
            assert run("check", "--lint", str(out)).returncode == 0, interval
    assert len(intervals()) == 20
    assert totals == PINNED_TOTALS


# ---- merge and check give one verdict ----

MERGE_CASES = {
    "closed": (f"{VS}\n{ps(key(2))}\n{pipeline(fnv(key(2)))}\n", True),
    "bad hash": (f"ps {'1' * 16} {key(2).hex()}\n", False),
    "out of range": (f"{ps(key(2, sampler_type=[6, 0, 0, 0]))}\n", False),
    "missing dependency": (f"{VS}\n{pipeline(fnv(key(2)))}\n", False),
    "lit record": (f"ps {fnv(key(2) + bytes(12))} {(key(2) + bytes(12)).hex()}\n", False),
}


@pytest.mark.parametrize("name", MERGE_CASES)
def test_merge_and_check_agree_on_every_case(tmp_path, name):
    text, accepted = MERGE_CASES[name]
    incoming = folder_with(tmp_path, text, sub="incoming")
    checked = run("check", "--lint", str(incoming)).returncode == 0
    lists = tmp_path / "lists"
    lists.mkdir()
    try:
        shader_lists.merge([incoming], lists)
        merged = True
    except SystemExit:
        merged = False
    assert checked == accepted
    assert merged == accepted


# ---- the struct is the table ----

XGPU = (ROOT / "port/linux/src/xgpu.h").read_text()


RENDER_STATES = ["D3DRS_PS_MAX", "D3DRS_PSCOMBINERCOUNT", "D3DRS_PSALPHAINPUTS0", "D3DRS_PSALPHAOUTPUTS0",
                 "D3DRS_PSRGBINPUTS0", "D3DRS_PSRGBOUTPUTS0"]


def render_state_enum():
    """the render states the key's layout depends on, with their values from xdk_pdb.h"""
    pdb = (ROOT / "port/include/xdk/xdk_pdb.h").read_text()
    values = {name: int(re.search(r"\b" + name + r" = (\d+)", pdb)[1]) for name in RENDER_STATES}
    return "enum { " + ", ".join(f"{name} = {value}" for name, value in values.items()) + " };\n"


def struct_translation_unit(source):
    """xgpu.h's key struct and its assertions, with the render states' values from xdk_pdb.h, as a unit of its own"""
    start = source.index("struct nv2a_pixel_shader_key\n{")
    end = source.index("#define XGPU_COVERAGE_ALPHA 1")
    return (f'#include <stddef.h>\n#include <stdint.h>\n#include "gpu.h"\n#include "{ROOT / "port/runtime/shader_record.h"}"\n'
            + "typedef uint32_t DWORD; /* 4 bytes, as in the guest */\n" + render_state_enum()
            + source[start:end] + "int main(void) { return 0; }\n")


def compiles(tmp_path, source):
    path = tmp_path / "key.c"
    path.write_text(struct_translation_unit(source))
    return subprocess.run(["cc", "-std=c11", "-fsyntax-only", f"-I{ROOT / 'port/linux/src'}", str(path)], capture_output=True, text=True)


def test_the_key_struct_matches_the_table(tmp_path):
    result = compiles(tmp_path, XGPU)
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("edit", [
    ("\tunsigned char coverage_alpha;\n", ""),  # a field the table lists is gone
    ("\tunsigned char fog_enable;\n", "\tunsigned char fog_enable;\n\tunsigned char spare;\n"),  # a field the table does not list
    ("\tuint32_t alpha_test_function;\n", "\tunsigned long long alpha_test_function;\n"),  # a field of another width
    ("\tDWORD texture_modes;\n", "\tDWORD texture_modes, texture_modes_too;\n"),
])
def test_a_struct_and_table_that_disagree_do_not_compile(tmp_path, edit):
    assert edit[0] in XGPU
    result = compiles(tmp_path, XGPU.replace(*edit, 1))
    assert result.returncode != 0 and "shader_record.h" in result.stderr


def test_the_pipeline_ranges_are_asserted_against_the_gpu_enums(tmp_path):
    changed = XGPU.replace("GPU_BLEND_ONE_MINUS_CONSTANT_ALPHA == SHADER_RECORD_FACTOR_MAX", "GPU_BLEND_ONE == SHADER_RECORD_FACTOR_MAX", 1)
    assert changed != XGPU
    result = compiles(tmp_path, changed)
    assert result.returncode != 0 and "pipeline ranges" in result.stderr


def test_xgpus_canonical_form_and_the_contracts_are_one_function(tmp_path):
    """xgpu.h writes the canonicalization out (its harnesses compile it alone); this holds it to shader_record.h's"""
    enum = render_state_enum()
    start = XGPU.index("static inline void nv2a_pixel_shader_key_canonicalize(")
    function = XGPU[start: XGPU.index("\n}\n", start) + 3]
    source = tmp_path / "canon.c"
    source.write_text(f'''#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t DWORD;
''' + f'#include "{ROOT / "port/runtime/shader_record.h"}"\n' + enum + '''
struct nv2a_pixel_shader_key { DWORD combiner_state[D3DRS_PS_MAX]; };
''' + function + '''
int main(void)
{
	srand(5);
	for (int round = 0; round < 20000; round++)
	{
		struct nv2a_pixel_shader_key a;
		uint32_t b[D3DRS_PS_MAX];
		for (int i = 0; i < D3DRS_PS_MAX; i++) a.combiner_state[i] = b[i] = (uint32_t)rand() * 2654435761u;
		if (round % 3) a.combiner_state[D3DRS_PSCOMBINERCOUNT] = b[D3DRS_PSCOMBINERCOUNT] = (uint32_t)(rand() % 12);
		nv2a_pixel_shader_key_canonicalize(&a);
		shader_record_canonicalize_combiners(b);
		if (memcmp(a.combiner_state, b, sizeof(b))) { printf("differ in round %d\\n", round); return 1; }
	}
	return 0;
}
''')
    binary = tmp_path / "canon"
    subprocess.run(["cc", "-std=c11", "-w", str(source), "-o", str(binary)], check=True)
    assert subprocess.run([str(binary)]).returncode == 0


# ---- the generated table is in the docs ----

def test_the_docs_hold_the_generated_field_table():
    docs = (ROOT / "docs/data-packs.md").read_text()
    found = re.search(r"<!-- shader-record fields -->\n(.*?)<!-- /shader-record fields -->", docs, re.S)
    assert found, "docs/data-packs.md has no generated shader-record field table"
    assert found[1].strip() == run("fields", "--markdown", check=True).stdout.strip()


# ---- the shader feed's released-build gate ----

TAGGED = bool(data_pack.released_shader_validators())


@pytest.mark.skipif(not TAGGED, reason="no release tags in this checkout")
def test_the_gate_passes_the_bundled_lists_and_refuses_a_sampler_type_of_six_naming_the_release(tmp_path):
    assert data_pack.validate_shaders_with_released_apps(ROOT / "port/shader-lists")[0] == "visr-v0.6.15"
    bad = folder_with(tmp_path, f"{ps(key(2, sampler_type=[6, 0, 0, 0]))}\n")
    with pytest.raises(ValueError, match="visr-v0.6.15 would refuse this shader snapshot"):
        data_pack.validate_shaders_with_released_apps(bad)


@pytest.mark.skipif(not TAGGED, reason="no release tags in this checkout")
def test_builds_before_the_floor_refuse_a_sampler_type_of_five_and_those_from_it_on_admit_it(tmp_path, monkeypatch):
    """why the floor is 0.6.15: 0.6.12 to 0.6.14 bound the sampler type at 4 and refuse a whole snapshot"""
    five = folder_with(tmp_path, f"{ps(key(2, sampler_type=[5, 0, 0, 0]))}\n")
    assert data_pack.validate_shaders_with_released_apps(five)[0] == "visr-v0.6.15"
    monkeypatch.setattr(data_pack, "SHADER_FEED_FLOOR_TAG", "visr-v0.6.12")
    with pytest.raises(ValueError, match=r"visr-v0\.6\.1[234] would refuse"):
        data_pack.validate_shaders_with_released_apps(five)


# ---- the tool's round trip ----

def random_key(rng):
    count = rng.randint(0, 8)
    words = [0] * 57
    words[53] = count | (rng.getrandbits(24) << 8)
    for base in (0, 26, 34, 45):
        for stage in range(count):
            words[base + stage] = rng.getrandbits(32)
    modes = sum(rng.randint(0, 18) << (5 * i) for i in range(4))
    tail = bytes(rng.randint(0, hi) for hi in (5, 5, 5, 5, 1, 1, 1, 1, 15, 15, 15, 15))
    compare = rng.choice([0, 0x200, 0x204, 0x207]).to_bytes(4, "little")
    rest = bytes([rng.randint(0, 1), rng.randint(0, 3), rng.randint(0, 1), rng.randint(0, 3)])
    return struct.pack("<57I", *words) + struct.pack("<I", modes) + tail + compare + rest


def test_records_written_in_range_round_trip_through_check_canonical_and_close(tmp_path):
    rng = random.Random(7)
    keys = [random_key(rng) for _ in range(60)]
    lines = [VS] + [ps(body) for body in keys]
    lines += [pipeline(fnv(body), fields=" ".join(str(rng.randint(0, hi)) for hi in (1, 14, 14, 4, 15, 15, 1)),
                       kinds="".join(f"{rng.randint(0, 19):02x}" for _ in range(16)),
                       alpha=f"{rng.randint(0, 14)} {rng.randint(0, 14)}") for body in keys]
    folder = folder_with(tmp_path, "\n".join(lines) + "\n")
    assert run("check", "--lint", str(folder)).returncode == 0
    canonical = tmp_path / "canonical"
    assert run("canonical", "--out", str(canonical), str(folder)).returncode == 0
    out, report = close(tmp_path, canonical)
    assert report["unresolved_pipelines"] == 0
    assert run("check", "--lint", str(out)).returncode == 0
    assert sorted(read_entries(out / "a10.txt")) == sorted(read_entries(canonical / "a10.txt"))


def test_whatever_close_reduces_a_lit_key_to_passes_the_validator(tmp_path):
    rng = random.Random(11)
    lits = []
    for _ in range(30):
        base = random_key(rng)
        lits.append(base + bytes([rng.randint(0, 1), rng.randint(0, 5), rng.randint(0, 3), rng.randint(0, 8)])
                    + struct.pack("<2I", rng.getrandbits(32) | 1, rng.getrandbits(32)))
    text = "\n".join([f"vs 00000000000000aa {variant} {'e' if variant % 2 == 0 else '0'}" for variant in (0, 1, 2, 3)]
                     + [f"ps {fnv(lit)} {lit.hex()}" for lit in lits]
                     + [pipeline(fnv(lit), vertex=f"00000000000000aa.{index % 4}.{'e' if index % 2 == 0 else '0'}")
                        for index, lit in enumerate(lits)]) + "\n"
    out, report = close(tmp_path, folder_with(tmp_path, text))
    assert report["unresolved_pipelines"] == 0
    assert run("check", "--lint", str(out)).returncode == 0
    assert all(len(line.split()[2]) == 504 for line in read_entries(out / "a10.txt") if line.startswith("ps "))


def test_ps_records_that_fold_to_one_key_keep_every_pipeline_closed(tmp_path):
    """two non-canonical keys with one canonical form: both recorded hashes must reach the one shader"""
    words = [0] * 57
    words[53] = 2
    one, two = bytearray(struct.pack("<57I", *words) + bytes(24)), bytearray(struct.pack("<57I", *words) + bytes(24))
    one[4 * 5:4 * 5 + 4] = (1).to_bytes(4, "little")
    two[4 * 5:4 * 5 + 4] = (2).to_bytes(4, "little")
    text = "\n".join([VS, ps(bytes(one)), ps(bytes(two)), pipeline(fnv(bytes(one))),
                      pipeline(fnv(bytes(two)), fields="1 4 5 0 15 0 1")]) + "\n"
    delta = folder_with(tmp_path, text)
    out, report = close(tmp_path, delta)
    assert report["unresolved_pipelines"] == 0
    assert len([line for line in read_entries(out / "a10.txt") if line.startswith("pipeline")]) == 2
    assert run("canonical", "--out", str(tmp_path / "canon"), str(delta)).returncode == 0


def test_close_refuses_a_ps_whose_key_does_not_hash_to_its_name(tmp_path):
    delta = folder_with(tmp_path, f"ps {'0' * 8}deadbeef {key(2).hex()}\n{pipeline('0' * 8 + 'deadbeef')}\n")
    result = run("close", "--out", str(tmp_path / "o"), str(delta))
    assert result.returncode == 1 and "the key hashes to another value" in result.stderr


def test_a_new_field_is_a_change_to_the_table_alone():
    """No code outside the field tables names a key or pipeline field, or its offset: lint, parse, format,
    admit, canonicalize and explain all walk the tables, so a field added to them needs no other edit."""
    header = (ROOT / "port/runtime/shader_record.h").read_text()
    names = set(re.findall(r"^\tX\((\w+),", header, re.M))
    assert {"blend", "alpha_destination", "combiner_state", "texture_modes", "sampler_type"} <= names
    offsets = set(re.findall(r"^\tX\(\w+, (\d{3}),", header, re.M))
    for path in ("port/runtime/shader_record.c", "port/runtime/shader_record_close.c",
                 "port/ios/tests/shader_record_tool.c"):
        text = re.sub(r'/\*.*?\*/|//[^\n]*', "", (ROOT / path).read_text(), flags=re.S)
        text = re.sub(r"#define ROW.*?\n(?:\t[^\n]*\n)*", "", text)
        for name in names:
            assert not re.search(rf'"{name}"|\br->{name}\b', text), f"{path} names the field {name}"
        for offset in offsets:
            assert not re.search(rf"\[{offset}\]|key\W[^;\n]*\b{offset}\b", text), f"{path} hard-codes the offset {offset}"


def test_pipeline_table_layout_the_parser_relies_on():
    """required fields first; the line is 4 + required (+ the optional ones) fields wide"""
    fields = run("fields", check=True).stdout.splitlines()
    pipeline = [line.split("\t") for line in fields if line.startswith("pipeline\t")]
    assert [row[1] for row in pipeline][:7] == ["blend", "source", "destination", "operation", "write_mask",
                                               "exact_borders", "depth"]
    assert [row[1] for row in pipeline].index("attribute_kinds") == 7
