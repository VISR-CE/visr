"""Exercise the real GL visibility functions with controlled GPU completion."""
from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("es,counters", [(False, False), (True, False), (True, True)])
def test_nonblocking_visibility_keeps_completed_counts(tmp_path, es, counters):
    compiler = shutil.which("cc")
    if compiler is None:
        pytest.skip("a C compiler is required")
    source = (ROOT / "port/linux/src/gpu_gl.c").read_text()
    state = source[source.index("static struct\n{\n\tGLuint queries[GPU_VISIBILITY_SLOTS];"):
                   source.index("\n#ifndef GPU_GL_ES\nstatic void GLAPIENTRY gl_debug_callback")]
    functions = source[source.index("/* Query availability covers"):
                       source.index("/* ---------- anti-aliasing passes")]
    harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef unsigned GLuint;
typedef unsigned GLenum;
typedef long GLintptr;
enum { GPU_VISIBILITY_SLOTS = 4096, GPU_VISIBILITY_NONBLOCKING = 1,
       GL_QUERY_RESULT_AVAILABLE = 10, GL_QUERY_RESULT, GL_QUERY_BUFFER,
       GL_ATOMIC_COUNTER_BUFFER, GL_STREAM_DRAW, GL_BUFFER_UPDATE_BARRIER_BIT,
       VISIBILITY_QUERY };
static unsigned halo_gl_call_count;
static GLuint next_query, next_buffer, active_query, last_buffer;
static unsigned begins, reads, barriers;
static GLuint ready[20000], values[20000], counter_values[20000], counter_query[20000];
static void glGenQueries(int n, GLuint *q) { assert(n == 1); *q = ++next_query; }
static void glBeginQuery(GLenum t, GLuint q) { (void)t; assert(!active_query); active_query=q; ready[q]=0; begins++; }
static void glEndQuery(GLenum t) { (void)t; counter_query[last_buffer]=active_query; active_query=0; }
static void glGetQueryObjectuiv(GLuint q, GLenum p, GLuint *value) {
    if (p == GL_QUERY_RESULT_AVAILABLE) *value=ready[q];
    else { assert(ready[q]); reads++; *value=values[q]; }
}
static void glBindBuffer(GLenum t, GLuint b) { (void)t; if (b) last_buffer=b; }
static void glGenBuffers(int n, GLuint *b) { assert(n == 1); *b=++next_buffer; }
static void glBufferData(GLenum t, size_t n, const void *v, GLenum usage) {
    (void)t; (void)usage; assert(n == sizeof(GLuint)); counter_values[last_buffer]=*(const GLuint *)v;
}
static void gl_buffer_write(GLenum t, GLintptr o, size_t n, const void *v) {
    (void)t; (void)o; (void)n; counter_values[last_buffer]=*(const GLuint *)v;
}
static GLuint gl_read_buffer_word(GLuint b, GLintptr o) {
    (void)o; assert(ready[counter_query[b]]); reads++; return counter_values[b];
}
static void glMemoryBarrier(GLenum bits) { assert(bits == GL_BUFFER_UPDATE_BARRIER_BIT); barriers++; }
'''
    program = harness + state + functions + r'''
static GLuint submit(unsigned slot, unsigned value) {
    GLuint query;
    gpu_gl_visibility_begin(GPU_VISIBILITY_NONBLOCKING);
    query=active_query;
    values[query]=value;
    counter_values[last_buffer]=value;
    gpu_gl_visibility_end(slot);
    return query;
}
int main(void) {
    GLuint samples=999, first, second;
#ifdef GPU_GL_ES
    visibility.counters=COUNTERS;
#endif
    first=submit(7, 31);
    assert(gpu_gl_visibility_result(7, &samples) == 1 && samples == 0 && reads == 0);
    ready[first]=1;
    assert(gpu_gl_visibility_result(7, &samples) == 1 && samples == 31);
    second=submit(7, 84);
    assert(gpu_gl_visibility_result(7, &samples) == 1 && samples == 31);
    ready[second]=1;
    assert(gpu_gl_visibility_result(7, &samples) == 1 && samples == 84);
    /* A full FIFO drops new work and keeps the last completed answer. */
    for (unsigned i=0; i<GPU_VISIBILITY_SLOTS; i++) submit(7, 2);
    unsigned before=begins, read_before=reads;
    submit(7, 3);
    assert(begins == before && visibility.pending_count == GPU_VISIBILITY_SLOTS);
    assert(gpu_gl_visibility_result(7, &samples) == 1 && samples == 84 && reads == read_before);
    /* Releasing the GPU drains the FIFO and storage can be reused. */
    memset(ready, 1, sizeof(ready));
    assert(gpu_gl_visibility_result(7, &samples) == 1 && samples == 2);
    assert(visibility.pending_count == 0);
    submit(7, 19);
    assert(visibility.pending_count == 1);
#ifdef GPU_GL_ES
    assert(!COUNTERS || barriers > 0);
#endif
    return 0;
}
'''
    path = tmp_path / "visibility.c"
    path.write_text(program)
    executable = tmp_path / "visibility"
    options = ["-DGPU_GL_ES=1"] if es else []
    subprocess.run([compiler, "-std=c11", f"-DCOUNTERS={int(counters)}", *options,
                    str(path), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
