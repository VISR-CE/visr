"""Zero-count host warming compiles programs before touching render resources."""
from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "port/ios/host/gpu_metal.m"


def compile_only_prefix(name):
    """Keep the real entry point through its first render-resource boundary."""
    source = SOURCE.read_text()
    start = source.index(name + "(")
    start = source.rfind("\n", 0, start) + 1
    end = source.index("MetalTexture *", start)
    prefix = source[start:end].replace("@autoreleasepool", "")
    # Metal-dependent work is unreachable for warming; flag entry into that tail.
    tail = "resource_accesses++;"
    if name == "host_soft_depth_prepare":
        tail += "return 99;"
    return prefix + tail + "}" * (prefix.count("{") - prefix.count("}"))


def run_probe(tmp_path, functions, main):
    probe = tmp_path / "host_prepare.c"
    probe.write_text("""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
typedef uint32_t gpu_texture;
static int program_calls, program_success, resource_accesses;
static struct { int active; } soft;
static int ssao_programs(void) { program_calls++; return program_success; }
static int soft_depth_program(void) { program_calls++; return program_success; }
""" + functions + "\nint main(void) {" + main + "return 0; }")
    binary = tmp_path / "host_prepare"
    subprocess.run(["cc", "-std=c11", str(probe), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


@pytest.mark.parametrize("success", [0, 1])
def test_ssao_zero_count_compiles_without_render_resources(tmp_path, success):
    run_probe(tmp_path, compile_only_prefix("host_ssao_apply"), f"""
        program_success = {success};
        soft.active = 1;
        host_ssao_apply(0, 0, NULL, 0);
        assert(program_calls == 1);
        assert(resource_accesses == 0);
        assert(soft.active == 1);
    """)


@pytest.mark.parametrize("success", [0, 1])
@pytest.mark.parametrize("active", [0, 1])
def test_soft_zero_count_preserves_active_state_and_reports_compile_result(tmp_path, success, active):
    run_probe(tmp_path, compile_only_prefix("host_soft_depth_prepare"), f"""
        program_success = {success};
        soft.active = {active};
        assert(host_soft_depth_prepare(0, NULL, 0) == {success});
        assert(program_calls == 1);
        assert(resource_accesses == 0);
        assert(soft.active == {active});
    """)


@pytest.mark.parametrize("name, arguments", [
    ("host_ssao_apply", "0, 0, NULL, 18"),
    ("host_soft_depth_prepare", "0, NULL, 5"),
])
def test_nonzero_count_continues_to_render_resource_lookup(tmp_path, name, arguments):
    run_probe(tmp_path, compile_only_prefix(name), f"""
        program_success = 1;
        {name}({arguments});
        assert(program_calls == 0);
        assert(resource_accesses == 1);
    """)
