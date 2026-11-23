"""The high-res text atlas goes up to the GPU at most once a frame (it is 16 MB of BGRA, and a menu that added a glyph
at each of its first draws put up 72 of them within a few frames, which ended the app on an Apple TV HD).

The test compiles the production `text_hires_begin_frame` and `text_hires_atlas_texture` against stubs that count
uploads, and checks the count over frames.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "port/linux/src/text_hires.c"


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


PROGRAM = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define ATLAS_SIZE 64
typedef unsigned int gpu_texture;
enum { GPU_TEXTURE_2D, GPU_FORMAT_BGRA8, GPU_USAGE_UPLOAD };
struct gpu_texture_description { int type, format, usage; unsigned width, height, depth, levels; };
static int uploads;
static gpu_texture gpu_texture_create(struct gpu_texture_description *d) { (void)d; return 7; }
static void gpu_texture_upload(gpu_texture t, int a, int b, void const *p, uint32_t n) { (void)t; (void)a; (void)b; (void)p; (void)n; uploads++; }
static unsigned char *atlas;
static int dirty_top = ATLAS_SIZE, dirty_bottom;
static unsigned long placeholder_data = 1;
static gpu_texture atlas_texture;
/* PRODUCTION */
int main(void)
{
	atlas = calloc((size_t)ATLAS_SIZE * ATLAS_SIZE, 1);
	text_hires_begin_frame();
	assert(text_hires_atlas_texture(1) == 7 && uploads == 1);
	/* glyphs added later in the same frame wait for the next one, however often the atlas is asked for */
	dirty_top = 0; dirty_bottom = 10;
	for (int i = 0; i < 5; i++) assert(text_hires_atlas_texture(1) == 7);
	assert(uploads == 1);
	text_hires_begin_frame();
	assert(text_hires_atlas_texture(1) == 7 && uploads == 2);
	/* a clean atlas is not uploaded */
	text_hires_begin_frame();
	for (int i = 0; i < 3; i++) text_hires_atlas_texture(1);
	assert(uploads == 2);
	return 0;
}
'''


def test_atlas_uploads_at_most_once_a_frame(tmp_path):
    source = SOURCE.read_text()
    production = "\n".join((
        "static unsigned long upload_frame = (unsigned long)-1, current_frame;",
        function(source, "void text_hires_begin_frame(void)\n"),
        function(source, "unsigned int text_hires_atlas_texture(unsigned long data)\n")))
    program = tmp_path / "atlas.c"
    program.write_text(PROGRAM.replace("/* PRODUCTION */", production))
    binary = tmp_path / "atlas"
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", str(program), "--output=" + str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)
