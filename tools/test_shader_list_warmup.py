"""Run the real captured-list parser with native GPU boundary stubs."""
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "port/linux/src/d3d8_device.c"


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def pipeline(vertex_hash, variant=0, blend="0 1 2 3 15 0 0"):
    return (f"pipeline {vertex_hash}.{variant}.0 2222222222222222 "
            f"{blend} 00000000000000000000000000000000 1 2")


def entries(vertex_hash, pixel_byte):
    return (f"vs {vertex_hash} 0 0\n"
            f"ps 2222222222222222 {pixel_byte + '00' * 15}\n"
            f"{pipeline(vertex_hash)}")


def test_captured_pipelines_warm_from_bundled_and_local_records():
    source = SOURCE.read_text()
    actual = "\n".join(function(source, signature) for signature in (
        "static int hex_digit(",
        "static BOOL hex_bytes(",
        "static struct vertex_shader_object *shader_list_program(",
        "static void lighting_key_fill(",
        "static int shader_list_soft_mode(",
        "static const struct nv2a_pixel_shader_key *shader_list_pixel_key(",
        "static BOOL shader_list_warm_remember(",
        "static BOOL per_pixel_lighting(",
        "void halo_shader_list_warm(",
    ))
    host_prepare = function(source, "static void shader_list_host_prepare(")
    actual = host_prepare + "\n" + actual
    probe = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu.h"
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define SHADER_LIST_RECORD_MAXIMUM (4L * 1024 * 1024)
struct vertex_shader_object {
    struct vertex_shader_object *next_object;
    uint64_t program_hash;
    void *instructions;
    gpu_shader shader[4];
    unsigned long shader_packed_mask[4];
    unsigned long packed_mask;
    BOOL lit;
};
#define NV2A_HIGHLIGHT_PRESETS 4
#define GRAPHICS_MODE_CLASSIC 1
#define GRAPHICS_MODE_ENHANCED 2
struct nv2a_pixel_shader_key {
    unsigned char byte, alpha_test_samples, soft_depth, lighting_highlight;
    unsigned char lighting_point_lights, pad[3];
    unsigned int lighting_program[2];
};
#define NV2A_PIXEL_SHADER_KEY_BASE_SIZE offsetof(struct nv2a_pixel_shader_key, alpha_test_samples)
#define NV2A_PIXEL_SHADER_KEY_SAMPLES_SIZE offsetof(struct nv2a_pixel_shader_key, lighting_program)
struct fragment_entry {
    struct fragment_entry *next;
    struct nv2a_pixel_shader_key key;
    gpu_shader shader;
};
#define FRAGMENT_BUCKETS 2
static struct fragment_entry *fragment_buckets[FRAGMENT_BUCKETS];
struct shader_list_warm_record {
    struct gpu_pipeline_description description;
    struct vertex_shader_object *program;
    struct nv2a_pixel_shader_key key;
    int variant;
};
static struct vertex_shader_object programs[2];
static struct vertex_shader_object *vertex_shader_objects = programs;
static struct {
    char map[256];
    struct { uint64_t hash; gpu_shader shader; } *aliases;
    unsigned long alias_count, alias_capacity;
} shader_list;
static struct { int gl_ready; unsigned long frame; } device;
static struct { int msl; } shader_dialect;
static struct { int vertex_bgra; } device_capabilities;
static int shader_list_warming;
static void nv2a_pixel_shader_key_canonicalize(struct nv2a_pixel_shader_key *key) { (void)key; }
''' + function((ROOT / 'port/linux/src/xgpu.h').read_text(), 'static inline unsigned long nv2a_pixel_shader_key_size(') + r'''
static uint64_t hash64(const void *bytes, unsigned long size) { (void)bytes; (void)size; return 0x2222222222222222ULL; }
static int warm_enabled = 1, mode_flags;
static long point_light_highlight, point_light_limit;
static long requested_highlight = 2, requested_limit = 5, alternate;
static long config_integer(const char *key) {
    if (!strcmp(key, "display.point_light_highlight")) return requested_highlight;
    if (!strcmp(key, "display.point_light_limit")) return requested_limit;
    return alternate;
}
static int config_boolean_in_graphics_mode(const char *key, int mode) {
    if (!strcmp(key, "display.per_pixel_lighting")) return mode == GRAPHICS_MODE_ENHANCED && (mode_flags & 1);
    if (!strcmp(key, "display.soft_particles")) return (mode_flags & 2) != 0;
    return !strcmp(key, "display.ambient_occlusion") && mode == GRAPHICS_MODE_ENHANCED && (mode_flags & 4);
}
static struct halo_stereo_frame stereo;
const struct halo_stereo_frame *halo_stereo_frame(void) { return &stereo; }
static unsigned int ssao_prepared, soft_prepared;
#ifdef HALO_ILP32
static int observation_capture, observation_developer_record;
static unsigned long observation_generation;
static void shader_list_take_pipelines(void) {}
static int host_shader_observation_recording(void) { return !!(mode_flags & 32); }
static int config_explicit(const char *key) { (void)key; return 0; }
static const char *cache_files_map_directory(void) { return "maps"; }
static const char *custom_edition_cache_loaded_path(void) { return ""; }
static int shader_observation_map_path(const char *map, const char *directory, const char *custom,
    char *path, size_t size) {
    (void)map; (void)directory; (void)custom; snprintf(path, size, "maps/test.map"); return 1;
}
static void platform_translate_path(const char *path, char *output, size_t size) { snprintf(output, size, "%s", path); }
static void host_perf_reports_identity(const char *path) { (void)path; }
static unsigned long host_shader_observation_activate(const char *path, int suppressed, char *key, size_t size) {
    (void)path; (void)suppressed; snprintf(key, size, "testmap"); return 1;
}
static int platform_fixed_timestep(void) { return 0; }
static double config_real(const char *key) { (void)key; return 0; }
#endif
void host_ssao_apply(gpu_texture color, gpu_texture depth, const float *values, uint32_t count) {
    assert(!color && !depth && !values && !count); ssao_prepared++;
}
uint32_t host_soft_depth_prepare(gpu_texture depth, const float *values, uint32_t count) {
    assert(!depth && !values && !count); soft_prepared++; return 1;
}
static const char *record_directory;
static unsigned char *bundled_text;
static uint32_t bundled_size;
static unsigned int vertex_count, pixel_count, pipeline_count;
static const uint64_t pixel_hash = 0x2222222222222222ULL;

static unsigned int read_file(const char *path, unsigned char **bytes) {
    FILE *file = fopen(path, "rb");
    long length;
    if (!file) return 0;
    assert(!fseek(file, 0, SEEK_END));
    length = ftell(file);
    assert(length >= 0);
    rewind(file);
    *bytes = malloc((size_t)length + 1);
    assert(*bytes);
    assert(fread(*bytes, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    return (unsigned int)length;
}
uint32_t gpu_warm_list_read(const char *map, char *destination, uint32_t size) {
    (void)map;
    if (!destination) return bundled_size;
    assert(size == bundled_size);
    memcpy(destination, bundled_text, size);
    return size;
}
static const char *config_string(const char *key) {
    if (!strcmp(key, "debug.shader_list_record")) return record_directory;
    return "";
}
static int config_boolean(const char *key) {
    if (!strcmp(key, "display.per_pixel_lighting")) return mode_flags & 1;
    return !strcmp(key, "debug.shader_list_warm") && warm_enabled;
}
static double halo_frame_trace_milliseconds(void) { return 0.0; }
static void platform_log(const char *format, ...) { (void)format; }
void gpu_warm_begin(void) {}
void gpu_warm_end(void) {}
static gpu_shader vertex_shader_compile(struct vertex_shader_object *program, int variant, unsigned long mask) {
    if (program->shader[variant]) return program->shader[variant];
    program->shader[variant] = (gpu_shader)(10 + vertex_count);
    program->shader_packed_mask[variant] = mask;
    vertex_count++;
    return program->shader[variant];
}
static gpu_shader fragment_shader_get(const struct nv2a_pixel_shader_key *key) {
    struct fragment_entry *entry;
    for (entry = fragment_buckets[0]; entry; entry = entry->next)
        if (!memcmp(&entry->key, key, sizeof(*key))) return entry->shader;
    pixel_count++;
    entry = calloc(1, sizeof(*entry));
    entry->key = *key;
    entry->shader = 7 + pixel_count;
    entry->next = fragment_buckets[0];
    fragment_buckets[0] = entry;
    return entry->shader;
}
static gpu_shader shader_list_pixel(uint64_t hash) {
    return hash == pixel_hash && fragment_buckets[0] ? fragment_buckets[0]->shader : 0;
}
void gpu_pipeline_warm(const struct gpu_pipeline_description *description) {
    assert(description->vertex_shader != 0);
    const struct fragment_entry *entry = fragment_buckets[0];
    while (entry && entry->shader != description->pixel_shader) entry = entry->next;
    assert(entry);
    int lit_vertex = 0, found_vertex = 0;
    for (int program = 0; program < 2; program++)
        for (int variant = 0; variant < 4; variant++)
            if (programs[program].shader[variant] == description->vertex_shader) {
                found_vertex = 1; lit_vertex = variant >= 2;
            }
    assert(found_vertex && lit_vertex == !!entry->key.lighting_program[0]);
    if (entry->key.lighting_program[0]) {
        assert(!entry->key.soft_depth);
        assert(entry->key.lighting_highlight == 2 && entry->key.lighting_point_lights == 3);
        assert(nv2a_pixel_shader_key_size(&entry->key) == sizeof(entry->key));
    }
    if (entry->key.soft_depth) assert(nv2a_pixel_shader_key_size(&entry->key) == 8);
    pipeline_count++;
}

''' + actual + r'''
static void check_helpers(void) {
    const int sources[] = { GPU_BLEND_SOURCE_ALPHA, GPU_BLEND_DESTINATION_COLOR, GPU_BLEND_DESTINATION_COLOR,
        GPU_BLEND_ONE, GPU_BLEND_ONE, GPU_BLEND_ONE, GPU_BLEND_ONE, GPU_BLEND_ONE };
    const int destinations[] = { GPU_BLEND_ONE_MINUS_SOURCE_ALPHA, GPU_BLEND_ZERO, GPU_BLEND_SOURCE_COLOR,
        GPU_BLEND_ONE, GPU_BLEND_ONE, GPU_BLEND_ONE, GPU_BLEND_ONE, GPU_BLEND_ONE_MINUS_SOURCE_ALPHA };
    const int operations[] = { GPU_BLEND_OP_ADD, GPU_BLEND_OP_ADD, GPU_BLEND_OP_ADD, GPU_BLEND_OP_ADD,
        GPU_BLEND_OP_REVERSE_SUBTRACT, GPU_BLEND_OP_MIN, GPU_BLEND_OP_MAX, GPU_BLEND_OP_ADD };
    const int modes[] = { 1, 2, 3, 4, 4, 2, 4, 5 };
    struct gpu_pipeline_description d = {0};
    struct nv2a_pixel_shader_key key = {0};
    int src, dst, op, row;
    d.blend = 1;
    for (src = GPU_BLEND_ZERO; src <= GPU_BLEND_ONE_MINUS_CONSTANT_ALPHA; src++)
        for (dst = GPU_BLEND_ZERO; dst <= GPU_BLEND_ONE_MINUS_CONSTANT_ALPHA; dst++)
            for (op = GPU_BLEND_OP_ADD; op <= GPU_BLEND_OP_MAX; op++) {
                int expected = 0;
                for (row = 0; row < 8; row++)
                    if (src == sources[row] && dst == destinations[row] && op == operations[row]) expected = modes[row];
                d.source = src; d.destination = dst; d.operation = op;
                assert(shader_list_soft_mode(&d) == expected);
            }
    d.source = sources[0]; d.destination = destinations[0]; d.operation = operations[0]; d.blend = 0;
    assert(!shader_list_soft_mode(&d));
    lighting_key_fill(&key, &programs[0], -1, 1);
    assert(!key.lighting_highlight && !key.lighting_point_lights);
    assert(key.lighting_program[0] == 0x11111111 && key.lighting_program[1] == 0x11111111);
    lighting_key_fill(&key, &programs[0], 99, 99);
    assert(key.lighting_highlight == NV2A_HIGHLIGHT_PRESETS && key.lighting_point_lights == 6);
    lighting_key_fill(&key, &programs[0], 2, 5);
    assert(key.lighting_highlight == 2 && key.lighting_point_lights == 3);
}
static void check_live_lighting(void) {
    struct nv2a_pixel_shader_key key = {0};
    int saved_flags = mode_flags;
    mode_flags = 1; device.frame = 10;
    assert(per_pixel_lighting());
    requested_highlight = requested_limit = 99;
    assert(per_pixel_lighting());
    lighting_key_fill(&key, &programs[0], point_light_highlight, point_light_limit);
    assert(key.lighting_highlight == 2 && key.lighting_point_lights == 3);
    device.frame++;
    assert(per_pixel_lighting());
    lighting_key_fill(&key, &programs[0], point_light_highlight, point_light_limit);
    assert(key.lighting_highlight == 4 && key.lighting_point_lights == 6);
    requested_highlight = 2; requested_limit = 5;
    alternate = 1; device.frame++;
    assert(!per_pixel_lighting());
    device.frame++;
    assert(per_pixel_lighting());
    alternate = 2; device.frame++;
    assert(per_pixel_lighting());
    lighting_key_fill(&key, &programs[0], point_light_highlight, point_light_limit);
    assert(!key.lighting_highlight && !key.lighting_point_lights);
    device.frame++;
    assert(per_pixel_lighting());
    lighting_key_fill(&key, &programs[0], point_light_highlight, point_light_limit);
    assert(key.lighting_highlight == 2 && key.lighting_point_lights == 3);
    mode_flags = 0; device.frame++;
    assert(!per_pixel_lighting());
    mode_flags = saved_flags; alternate = 0;
}
int main(int argc, char **argv) {
    unsigned int expected_vertices, expected_pixels, expected_pipelines;
    assert(argc == 6 || argc == 7);
    if (argc == 7) mode_flags = atoi(argv[6]);
    bundled_size = read_file(argv[1], &bundled_text);
    record_directory = argv[2];
    expected_vertices = (unsigned int)strtoul(argv[3], NULL, 10);
    expected_pixels = (unsigned int)strtoul(argv[4], NULL, 10);
    expected_pipelines = (unsigned int)strtoul(argv[5], NULL, 10);
    programs[0].next_object = &programs[1];
    programs[0].program_hash = 0x1111111111111111ULL;
    programs[0].instructions = &programs[0];
    programs[1].program_hash = 0x3333333333333333ULL;
    programs[1].instructions = &programs[1];
    programs[0].lit = TRUE;
    check_helpers();
    check_live_lighting();
    device.gl_ready = 1;
    shader_dialect.msl = !(mode_flags & 16);
    stereo.mode = mode_flags & 8 ? HALO_STEREO_HEAD : 0;
    halo_shader_list_warm("levels/test/testmap");
    assert(vertex_count == expected_vertices);
    assert(pixel_count == expected_pixels);
    assert(pipeline_count == expected_pipelines);
    printf("PASS: %u vertex, %u pixel, %u pipeline warmups\n", vertex_count, pixel_count, pipeline_count);
#ifdef HALO_ILP32
    assert(ssao_prepared == (shader_dialect.msl && (mode_flags & 4) ? 1u : 0u));
    assert(soft_prepared == (shader_dialect.msl && (mode_flags & 2) && !(mode_flags & 8) ? 1u : 0u));
#else
    assert(!ssao_prepared && !soft_prepared);
#endif
    while (fragment_buckets[0]) {
        struct fragment_entry *entry = fragment_buckets[0];
        fragment_buckets[0] = entry->next;
        free(entry);
    }
    free(shader_list.aliases);
    free(bundled_text);
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        c_file, binary = temp / "probe.c", temp / "probe"
        c_file.write_text(probe)

        cases = (
            ("bundled-and-local", f"# bundled\n\n{entries('1111111111111111', '00')}",
             f"# local\n\n{entries('3333333333333333', '01')}", (2, 2, 2)),
            ("bundled-and-local-trailing-newlines",
             f"# bundled\n\n{entries('1111111111111111', '00')}\n",
             f"# local\n\n{entries('3333333333333333', '01')}\n", (2, 2, 2)),
            ("local-only", "", f"# local only\n\n{entries('3333333333333333', '01')}", (1, 1, 1)),
            ("bundled-only", f"# bundled only\n\n{entries('1111111111111111', '00')}", "", (1, 1, 1)),
            ("empty", "", "", (0, 0, 0)),
            ("empty-host-programs", "", "", (0, 0, 0), 7),
            ("lit-derived", entries('1111111111111111', '00'), "", (2, 2, 2), 1),
            ("unlit-program", entries('3333333333333333', '00'), "", (1, 1, 1), 1),
            ("soft-derived", entries('1111111111111111', '00').replace(
                "0 1 2 3 15 0 0", "1 4 5 0 15 0 1"), "", (1, 2, 2), 2),
            ("both-derived", entries('1111111111111111', '00').replace(
                "0 1 2 3 15 0 0", "1 4 5 0 15 0 1"), "", (2, 3, 3), 7),
            ("observation-capture", entries('1111111111111111', '00').replace(
                "0 1 2 3 15 0 0", "1 4 5 0 15 0 1"), "", (2, 3, 3), 39),
            ("head-stereo", entries('1111111111111111', '00').replace(
                "0 1 2 3 15 0 0", "1 4 5 0 15 0 1"), "", (2, 2, 2), 15),
            ("gl", entries('1111111111111111', '00'), "", (1, 1, 1), 23),
            ("soft-no-depth", entries('1111111111111111', '00').replace(
                "0 1 2 3 15 0 0", "1 4 5 0 15 0 0"), "", (1, 1, 1), 2),
            ("soft-unknown-blend", entries('1111111111111111', '00').replace(
                "0 1 2 3 15 0 0", "1 1 0 0 15 0 1"), "", (1, 1, 1), 2),
            ("lit-record", "vs 1111111111111111 2 0\n"
             "ps 2222222222222222 00000002030000001111111111111111\n" + pipeline('1111111111111111', 2),
             "", (2, 2, 2), 1),
            ("lit-immediate-derived", entries('1111111111111111', '00').replace(
                '1111111111111111 0 0', '1111111111111111 1 0').replace(
                '1111111111111111.0.0', '1111111111111111.1.0'), "", (2, 2, 2), 1),
            ("lit-immediate-record", "vs 1111111111111111 3 0\n"
             "ps 2222222222222222 00000002030000001111111111111111\n" + pipeline('1111111111111111', 3),
             "", (2, 2, 2), 1),
            ("lit-record-disabled", "vs 1111111111111111 2 0\n"
             "ps 2222222222222222 00000002030000001111111111111111\n" + pipeline('1111111111111111', 2),
             "", (2, 2, 2), 0),
            ("lit-record-gl", "vs 1111111111111111 2 0\n"
             "ps 2222222222222222 00000002030000001111111111111111\n" + pipeline('1111111111111111', 2),
             "", (0, 1, 0), 16),
            ("lit-record-soft", "vs 1111111111111111 2 0\n"
             "ps 2222222222222222 00000002030000001111111111111111\n" +
             pipeline('1111111111111111', 2, '1 4 5 0 15 0 1'), "", (2, 3, 3), 3),
            ("duplicate-derived", entries('1111111111111111', '00'),
             entries('1111111111111111', '00'), (2, 2, 3), 1),
        )
        for defines in ([], ["-DHALO_ILP32=1"]):
            subprocess.run(["xcrun", "clang", "-Wall", "-Werror", "-O1",
                            "-fsanitize=address,undefined", "-Iport/linux/src", *defines, str(c_file), "-o", str(binary)], check=True)
            for name, bundled, local, expected, *flags in cases:
                case = temp / name
                records = case / "records"
                records.mkdir(parents=True, exist_ok=True)
                bundle_file = case / "bundled.txt"
                bundle_file.write_text(bundled)
                if local:
                    (records / "testmap.txt").write_text(local)
                subprocess.run([str(binary), str(bundle_file), str(records),
                                *(str(value) for value in expected), *(str(value) for value in flags)], check=True)


if __name__ == "__main__":
    test_captured_pipelines_warm_from_bundled_and_local_records()
