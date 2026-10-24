"""Exercise repeated-draw keys and texture fast paths without a GPU device."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def run_probe(tmp_path, source):
    path = tmp_path / "probe.c"
    path.write_text(source)
    binary = tmp_path / "probe"
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_pixel_key_reuses_conversion_and_tracks_every_source(tmp_path):
    source = (ROOT / "port/linux/src/d3d8_device.c").read_text()
    actual = function(source, "static void pixel_shader_key_fill(")
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t DWORD;
typedef int BOOL;
#define TRUE 1
#define D3DTSS_MAXSTAGES 4
#define D3DRS_PS_MAX 57
#define D3DRS_PSCONSTANT0_0 9
#define D3DRS_PSFINALCOMBINERCONSTANT0 25
#define D3DRS_PSFINALCOMBINERCONSTANT1 26
enum { D3DRS_PSTEXTUREMODES=57, D3DRS_ALPHATESTENABLE, D3DRS_ALPHAFUNC,
    D3DRS_FOGENABLE, D3DRS_FOGTABLEMODE };
enum { D3DTSS_ALPHAKILL, D3DTSS_COLORSIGN, D3DTALPHAKILL_ENABLE=1, GPU_OCCLUSION_SHADER_COUNTER=2 };
struct nv2a_pixel_shader_key {
    DWORD combiner_state[D3DRS_PS_MAX], texture_modes;
    unsigned char sampler_type[4], alpha_kill[4], color_sign[4];
    unsigned long alpha_test_function;
    unsigned char fog_enable, fog_table_mode, count_samples, coverage_alpha;
};
static DWORD D3D__RenderState[64], D3D__TextureState[4][2];
static struct { int visibility_test_active; } device;
static struct { int occlusion_mode; } device_capabilities;
static int conversions;
static int words_equal(const void *a, const void *b, unsigned long size) { return !memcmp(a,b,size); }
static void *observe_memset(void *p, int value, unsigned long size) { conversions++; return memset(p,value,size); }
#undef memset
#define memset observe_memset
''' + actual + r'''
#undef memset
static void check(void) {
    struct nv2a_pixel_shader_key actual, expected;
    memset(&expected,0,sizeof(expected));
    memcpy(expected.combiner_state,D3D__RenderState,sizeof(expected.combiner_state));
    memset(&expected.combiner_state[D3DRS_PSCONSTANT0_0],0,16*sizeof(DWORD));
    expected.combiner_state[D3DRS_PSFINALCOMBINERCONSTANT0]=0;
    expected.combiner_state[D3DRS_PSFINALCOMBINERCONSTANT1]=0;
    expected.texture_modes=D3D__RenderState[D3DRS_PSTEXTUREMODES];
    for (int i=0;i<4;i++) {
        expected.alpha_kill[i]=D3D__TextureState[i][D3DTSS_ALPHAKILL]==D3DTALPHAKILL_ENABLE;
        expected.color_sign[i]=(D3D__TextureState[i][D3DTSS_COLORSIGN]>>28)&15;
    }
    expected.alpha_test_function=D3D__RenderState[D3DRS_ALPHATESTENABLE] ? D3D__RenderState[D3DRS_ALPHAFUNC] : 0;
    expected.fog_enable=D3D__RenderState[D3DRS_FOGENABLE]!=0;
    expected.fog_table_mode=(unsigned char)D3D__RenderState[D3DRS_FOGTABLEMODE];
    expected.count_samples=device.visibility_test_active && device_capabilities.occlusion_mode==GPU_OCCLUSION_SHADER_COUNTER;
    pixel_shader_key_fill(&actual);
    assert(!memcmp(&actual,&expected,sizeof(actual)));
    int before=conversions;
    /* Stage resolution and companion adaptation modify only the caller's copy. */
    actual.coverage_alpha=3; actual.sampler_type[0]=2; actual.combiner_state[0]=123;
    pixel_shader_key_fill(&actual);
    assert(conversions==before && !memcmp(&actual,&expected,sizeof(actual)));
}
int main(void) {
    check();
    for (int i=0;i<62;i++) { D3D__RenderState[i]=0x12345601; check(); }
    for (int i=0;i<4;i++) {
        D3D__TextureState[i][D3DTSS_ALPHAKILL]=1; check();
        D3D__TextureState[i][D3DTSS_COLORSIGN]=0xf0000000; check();
    }
    device.visibility_test_active=1; check();
    device_capabilities.occlusion_mode=GPU_OCCLUSION_SHADER_COUNTER; check();
    device.visibility_test_active=0; check();
}
''')


def test_stage_texture_fast_path_checks_metadata_writes_and_lifetime(tmp_path):
    source = (ROOT / "port/linux/src/xbox_textures.c").read_text()
    declarations = source[source.index("struct recent_texture\n"):source.index("static unsigned long texture_drop_serial")]
    # The slow lookup/upload boundary is a sentinel: every invalid fast entry
    # must reach it, while hits must still call texture_entry_result.
    start = source.index("gpu_texture xgpu_texture_get(")
    end = source.index("\tbucket = &texture_buckets[bucket_number];", start)
    actual = source[start:end] + r'''
    (void)bucket; (void)generation; (void)hash; (void)oldest_variant;
    (void)variant_count; (void)no_cache; slow_lookups++; return 0;
}
'''
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef uint32_t DWORD;
typedef uint32_t D3DCOLOR;
typedef unsigned gpu_texture;
typedef int BOOL;
#define D3DTSS_MAXSTAGES 4
#define RECENT_TEXTURE_COUNT 512
#define D3DFORMAT_FORMAT_MASK 0xff
#define D3DFORMAT_FORMAT_SHIFT 0
struct texture_entry { unsigned long last_used_frame; gpu_texture texture; };
struct xgpu_texture_description { unsigned levels; };
static unsigned long texture_frame=7, texture_drop_serial=1, watch_serial=1;
static unsigned hash_calls, result_calls, slow_lookups;
static unsigned long memory_watch_serial(void) { return watch_serial; }
static unsigned long palette_hash(const D3DCOLOR *p) { (void)p; return 0; }
static unsigned long bucket_index(DWORD data, DWORD format, DWORD size) {
    hash_calls++; return (data^format^size)&4095;
}
static gpu_texture texture_entry_result(struct texture_entry *entry, uint32_t *type,
    struct xgpu_texture_description *description) {
    result_calls++; *type=1; description->levels=1; return entry->texture;
}
''' + declarations + actual + r'''
int main(void) {
    DWORD resource[5]={0,123,0,4,64};
    struct texture_entry entry={0,88};
    struct xgpu_texture_description description;
    uint32_t type;
    unsigned long index=bucket_index(resource[1],resource[3],resource[4])%512;
    recent_textures[index]=(struct recent_texture){123,4,64,&entry,1,1};
    hash_calls=0;
    assert(xgpu_texture_get(resource,NULL,&type,&description,0)==88);
    assert(hash_calls==1 && result_calls==1 && entry.last_used_frame==7);
    entry.texture=89; texture_frame++;
    assert(xgpu_texture_get(resource,NULL,&type,&description,0)==89);
    assert(hash_calls==1 && result_calls==2 && entry.last_used_frame==8);
    for (int i=0;i<3;i++) {
        int field=(int[]){1,3,4}[i];
        resource[field]++;
        assert(xgpu_texture_get(resource,NULL,&type,&description,0)==0);
        resource[field]--;
    }
    watch_serial++;
    assert(xgpu_texture_get(resource,NULL,&type,&description,0)==0);
    watch_serial--;
    texture_drop_serial++;
    assert(xgpu_texture_get(resource,NULL,&type,&description,0)==0);
    texture_drop_serial--;
    /* The palette must be considered on every palettized bind. */
    resource[3]=11;
    stage_recent_textures[0].format_word=11;
    assert(xgpu_texture_get(resource,NULL,&type,&description,0)==0);
    resource[3]=4;
    /* Settings changes clear both tiers; a surviving shared hit can promote. */
    memset(stage_recent_textures,0,sizeof(stage_recent_textures));
    assert(xgpu_texture_get(resource,NULL,&type,&description,1)==89);
    unsigned before=hash_calls;
    assert(xgpu_texture_get(resource,NULL,&type,&description,1)==89 && hash_calls==before);
    memset(stage_recent_textures,0,sizeof(stage_recent_textures));
    memset(recent_textures,0,sizeof(recent_textures));
    assert(xgpu_texture_get(resource,NULL,&type,&description,1)==0);
    assert(slow_lookups==7);
}
''')
