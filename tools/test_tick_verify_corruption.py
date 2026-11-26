"""debug.verify_once_a_tick puts a block cache's and a memory pool's list walk off to the end of the tick. The class
of bug is a list gone wrong inside a tick that the game survives past the end of that tick: the block was freed, or
the pool compacted, before the put-off walk could see it. The probe builds the real `lruv_cache.c`, `memory_pool.c`
and `tick_verify.c`, goes wrong inside a tick in each way and reports where the assertion stopped it. With the
setting off the assertions fall exactly where they fall without `tick_verify.c`."""
import re
import shutil
import subprocess
from probe_timeouts import PROBE_RUN_SECONDS
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]

CSERIES = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
typedef float real;
typedef unsigned char byte;
#define FALSE 0
#define TRUE 1
#define NONE -1
#define SHORT_BITS 16
#define TAG_STRING_LENGTH 31
#define NUMBEROF(array) (sizeof(array) / sizeof((array)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define csmemset memset
#define csmemcpy memcpy
#define csmemmove memmove
#define csstrncpy strncpy
#define csprintf sprintf
#define UNUSED_PARAMETER(parameter) (void)(parameter)
extern char temporary[256];
void probe_assert(const char *expression);
#define match_assert(file, line, expression) if (!(expression)) { probe_assert(#expression); }
#define match_vassert(file, line, expression, string) if (!(expression)) { probe_assert(#expression); }
#define match_warn(file, line, expression) (void)0
#define match_vwarn(file, line, expression, string) (void)0
#define match_malloc(file, line, size) malloc(size)
#define match_free(file, line, pointer) free(pointer)
'''

DATA = r'''
struct data_array { short maximum_count, size, actual_count; char *used; void *data; };
struct data_iterator { struct data_array *data; short absolute_index; long datum_index; };
long data_allocation_size(short maximum_count, short size);
void data_initialize(struct data_array *data, const char *name, short maximum_count, short size);
void data_make_valid(struct data_array *data);
void data_dispose(struct data_array *data);
void data_verify(struct data_array *data);
void *datum_get(struct data_array *data, long index);
long datum_new(struct data_array *data);
void datum_delete(struct data_array *data, long index);
void data_iterator_new(struct data_iterator *iterator, struct data_array *data);
void *data_iterator_next(struct data_iterator *iterator);
'''

PROBE = r'''
#include "cseries/cseries.h"
#include "memory/data.h"
#include "memory/lruv_cache.h"
#include "memory_pool.h"

char temporary[256];
static const char *phase = "before";
static boolean setting;

void probe_assert(const char *expression) { (void)expression; printf("%s\n", phase); exit(0); }
boolean sim_experiment_enabled(short experiment) { return experiment == 4 && setting; }
void tick_verify_begin(void);
void tick_verify_end(void);

/* a datum array enough for the cache's blocks */
long data_allocation_size(short maximum_count, short size) { return sizeof(struct data_array) + maximum_count * (size + 1); }
void data_initialize(struct data_array *data, const char *name, short maximum_count, short size)
{
    (void)name;
    data->maximum_count = maximum_count; data->size = size; data->actual_count = 0;
    data->used = (char *)(data + 1); data->data = data->used + maximum_count;
    memset(data->used, 0, maximum_count);
}
void data_make_valid(struct data_array *data) { (void)data; }
void data_dispose(struct data_array *data) { (void)data; }
void data_verify(struct data_array *data) { (void)data; }
void *datum_get(struct data_array *data, long index)
{
    if (index < 0 || index >= data->maximum_count || !data->used[index]) probe_assert("datum_get");
    return (char *)data->data + index * data->size;
}
long datum_new(struct data_array *data)
{
    long index;
    for (index = 0; index < data->maximum_count; index++)
        if (!data->used[index]) { data->used[index] = 1; data->actual_count++; memset(datum_get(data, index), 0, data->size); return index; }
    return NONE;
}
void datum_delete(struct data_array *data, long index) { datum_get(data, index); data->used[index] = 0; data->actual_count--; }
void data_iterator_new(struct data_iterator *iterator, struct data_array *data) { iterator->data = data; iterator->absolute_index = 0; iterator->datum_index = NONE; }
void *data_iterator_next(struct data_iterator *iterator)
{
    while (iterator->absolute_index < iterator->data->maximum_count)
    {
        long index = iterator->absolute_index++;
        if (iterator->data->used[index]) { iterator->datum_index = index; return datum_get(iterator->data, index); }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    const char *subject = argv[1], *wrong = argv[2], *then = argv[3];

    (void)argc;
    setting = argv[4][0] == '1';
    if (!strcmp(subject, "pool"))
    {
        struct memory_pool *pool = memory_pool_new("probe", 4096);
        void *references[4] = { NULL, NULL, NULL, NULL };
        struct memory_pool_block *block;
        int index;

        for (index = 0; index < 3; index++) memory_pool_block_allocate(pool, &references[index], 64);
        tick_verify_begin();
        phase = "use"; memory_pool_block_allocate(pool, &references[3], 64); /* (the pool is used this tick) */
        block = (struct memory_pool_block *)references[1] - 1;
        if (!strcmp(wrong, "signature")) block->header_signature = 0;
        else if (!strcmp(wrong, "link")) block->previous_block = block;
        else if (!strcmp(wrong, "last-next")) pool->last_block->next_block = block; /* the end of the list goes on */
        else pool->last_block = block; /* "last": the list ends early */
        if (!strcmp(then, "free")) { phase = "free"; memory_pool_block_free(pool, &references[1]); }
        if (!strcmp(then, "compact")) { phase = "compact"; memory_pool_compact(pool); }
        if (!strcmp(then, "allocate")) { void *another = NULL; phase = "allocate"; memory_pool_block_allocate(pool, &another, 64); }
        phase = "end"; tick_verify_end();
        phase = "next"; memory_pool_block_free(pool, &references[0]);
    }
    else
    {
        struct lruv_cache *cache = lruv_new("probe", 64, 4, 16, NULL, NULL);
        long blocks[4];
        struct lruv_cache_block *block;
        int index;

        for (index = 0; index < 3; index++) blocks[index] = lruv_block_new(cache, 32);
        tick_verify_begin();
        phase = "use"; blocks[3] = lruv_block_new(cache, 32);
        block = datum_get(cache->blocks, blocks[1]);
        block = block->previous_block_index == NONE ? datum_get(cache->blocks, blocks[2]) : block; /* (one with a block before it) */
        if (!strcmp(wrong, "link")) block->previous_block_index = NONE;
        else block->first_page_index = -1000; /* "pages" */
        if (!strcmp(then, "free")) { phase = "free"; lruv_block_delete(cache, block == datum_get(cache->blocks, blocks[1]) ? blocks[1] : blocks[2]); }
        phase = "end"; tick_verify_end();
        phase = "next"; lruv_block_new(cache, 32);
    }
    printf("never\n");
    return 0;
}
'''


@pytest.fixture(scope='module')
def probe(tmp_path_factory):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the probe')
    folder = tmp_path_factory.mktemp('tick_verify_corruption')
    for name in ('cseries', 'memory'):
        (folder / name).mkdir()
    (folder / 'cseries/cseries.h').write_text(CSERIES)
    (folder / 'cseries.h').write_text('#include "cseries/cseries.h"\n')
    (folder / 'memory/data.h').write_text(DATA)
    # (the cache's header as it is, without the two assertions on the 32-bit game's structure sizes)
    header = (ROOT / 'source/memory/lruv_cache.h').read_text(errors='surrogateescape')
    header, dropped = re.subn(r'typedef char lruv_cache\w*_size_assert\[[^;]*;', '', header)
    assert dropped == 2
    (folder / 'memory/lruv_cache.h').write_text(header, errors='surrogateescape')
    (folder / 'probe.c').write_text(PROBE)
    binary = folder / 'probe'
    subprocess.run([compiler, '-w', '-fsanitize=undefined', '--include-directory=' + str(folder),
                    '--include-directory=' + str(ROOT / 'source/memory'), str(folder / 'probe.c'),
                    str(ROOT / 'source/memory/lruv_cache.c'), str(ROOT / 'source/memory/memory_pool.c'),
                    str(ROOT / 'port/linux/game/tick_verify.c'), '-o', str(binary), '-lm'], check=True)

    def run(subject, wrong, then, setting):
        return subprocess.run([str(binary), subject, wrong, then, '1' if setting else '0'], check=True,
                              capture_output=True, text=True, timeout=PROBE_RUN_SECONDS).stdout.strip()
    return run


CASES = [(subject, wrong, then)
         for subject, wrongs, thens in (('pool', ('signature', 'link'), ('free', 'compact', 'leave')),
                                        ('cache', ('link', 'pages'), ('free', 'leave')))
         for wrong in wrongs for then in thens]
# an allocation links the new block to the end of the list, which would write over a wrong link there
CASES += [('pool', 'last-next', 'allocate'), ('pool', 'last', 'allocate'), ('pool', 'signature', 'allocate')]


@pytest.mark.parametrize('subject,wrong,then', CASES)
def test_a_list_gone_wrong_inside_a_tick_stops_the_game_by_the_end_of_that_tick(probe, subject, wrong, then):
    # freed, compacted, allocated after or left alone: stopped where that happens or where the tick ends, never later
    assert probe(subject, wrong, then, True) in {'free': ('free',), 'compact': ('compact',), 'leave': ('end',),
                                                  'allocate': ('allocate', 'end')}[then]


# where the assertion falls with the setting off: the game's own timing, measured on the files before
# `tick_verify.c` existed. A pool compacted with a wrong link is repaired by the compaction and never stops; a list
# left alone is not walked until its next use.
WITHOUT = {
    ('pool', 'signature', 'free'): 'free', ('pool', 'signature', 'compact'): 'compact',
    ('pool', 'signature', 'leave'): 'next', ('pool', 'link', 'free'): 'free', ('pool', 'link', 'compact'): 'never',
    ('pool', 'link', 'leave'): 'next', ('cache', 'link', 'free'): 'free', ('cache', 'link', 'leave'): 'next',
    ('cache', 'pages', 'free'): 'free', ('cache', 'pages', 'leave'): 'next',
    ('pool', 'last-next', 'allocate'): 'allocate', ('pool', 'last', 'allocate'): 'allocate',
    ('pool', 'signature', 'allocate'): 'allocate',
}


@pytest.mark.parametrize('subject,wrong,then', CASES)
def test_with_the_setting_off_the_assertions_fall_where_they_always_did(probe, subject, wrong, then):
    assert probe(subject, wrong, then, False) == WITHOUT[(subject, wrong, then)]
