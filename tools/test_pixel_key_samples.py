"""Build pixel shader keys with the renderer's real `pixel_key_finish` and canonicalizer.

A multisampled, alpha-tested, non-blended draw must carry `alpha_test_samples`
(MSAA edge coverage for alpha-tested surfaces), and canonicalizing the key must
keep it. Blended, single-sample and sample-mask-less draws must not get it.
The GPU is replaced by stubs; no game data is needed.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def main():
    device = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    xgpu = (ROOT / "port/linux/src/xgpu.h").read_text()
    finish = function(device, "static void pixel_key_finish(")
    canonicalize = function(xgpu, "static inline void nv2a_pixel_shader_key_canonicalize(")
    program = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef unsigned long DWORD;
typedef int BOOL;
enum { D3DRS_PSCOMBINERCOUNT, D3DRS_PSALPHAINPUTS0, D3DRS_PSALPHAOUTPUTS0 = D3DRS_PSALPHAINPUTS0 + 8,
	D3DRS_PSRGBINPUTS0 = D3DRS_PSALPHAOUTPUTS0 + 8, D3DRS_PSRGBOUTPUTS0 = D3DRS_PSRGBINPUTS0 + 8,
	D3DRS_ALPHABLENDENABLE = D3DRS_PSRGBOUTPUTS0 + 8, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_ALPHATESTENABLE,
	D3DRS_ALPHAFUNC, D3DRS_FOGENABLE, D3DRS_FOGTABLEMODE, D3DRS_MAX };
enum { D3DTSS_ALPHAKILL, D3DTSS_COLORSIGN, D3DTSS_MAXSTAGES = 4 };
#define D3DTALPHAKILL_ENABLE 1
#define D3DBLEND_CONSTANTCOLOR 1
#define D3DBLEND_SRCALPHA 2
#define XGPU_COVERAGE_ALPHA 1
#define GPU_OCCLUSION_SHADER_COUNTER 1
struct nv2a_pixel_shader_key {
	DWORD combiner_state[D3DRS_MAX];
	unsigned char alpha_kill[D3DTSS_MAXSTAGES], color_sign[D3DTSS_MAXSTAGES];
	unsigned char coverage_alpha, alpha_test_function, alpha_test_samples, fog_enable, fog_table_mode, count_samples;
};
static DWORD D3D__RenderState[D3DRS_MAX];
static DWORD D3D__TextureState[D3DTSS_MAXSTAGES][2];
static uint32_t target_samples;
static struct { int sample_mask; } shader_dialect;
static struct { int visibility_test_active; } device;
static struct { int occlusion_mode; } device_capabilities;
''' + canonicalize + "\n" + finish + r'''
static unsigned char build(uint32_t samples, int sample_mask, int blend)
{
	struct nv2a_pixel_shader_key key;
	memset(&key, 0, sizeof(key));
	memset(D3D__RenderState, 0, sizeof(D3D__RenderState));
	D3D__RenderState[D3DRS_PSCOMBINERCOUNT] = 2;
	D3D__RenderState[D3DRS_PSRGBINPUTS0 + 5] = 0x1234;    /* stale, past the count */
	D3D__RenderState[D3DRS_ALPHATESTENABLE] = 1;
	D3D__RenderState[D3DRS_ALPHAFUNC] = 5;
	D3D__RenderState[D3DRS_ALPHABLENDENABLE] = blend;
	memcpy(key.combiner_state, D3D__RenderState, sizeof(key.combiner_state));
	target_samples = samples;
	shader_dialect.sample_mask = sample_mask;
	pixel_key_finish(&key);
	assert(key.combiner_state[D3DRS_PSRGBINPUTS0 + 5] == 0);    /* canonicalized */
	assert(key.alpha_test_function == 5);
	return key.alpha_test_samples;
}

int main(void)
{
	assert(build(4, 1, 0) == 4);    /* multisampled, alpha-tested, not blended: kept through canonicalizing */
	assert(build(2, 1, 0) == 2);
	assert(build(1, 1, 0) == 0);    /* single sample */
	assert(build(4, 0, 0) == 0);    /* no sample mask in the shader dialect */
	assert(build(4, 1, 1) == 0);    /* blended */
	return 0;
}
'''
    if os.uname().sysname != "Darwin":
        print("SKIP: the probe compiles with Xcode's clang and its sanitizers")
        return
    with tempfile.TemporaryDirectory() as directory:
        source, binary = Path(directory) / "probe.c", Path(directory) / "probe"
        source.write_text(program)
        subprocess.run(["xcrun", "clang", "-Wall", "-Werror", "-Wno-unused-variable", "-Wno-unused-function",
                        "-fsanitize=address,undefined", str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print("PASS: multisampled alpha-tested keys carry alpha_test_samples through canonicalizing")


main()
