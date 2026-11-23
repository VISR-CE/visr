"""debug.verify_once_a_tick (port/linux/game/tick_verify.c) may put a cache's or pool's list walk off, never drop it:
every subject that asked inside a tick or frame is walked exactly once when it ends, a subject going
away is walked before it goes, and outside a tick, with the setting off or with no room left the caller walks at
once. The class of bug is a verification asked for and never made."""
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_a_walk_put_off_is_made_once_by_the_end_of_the_tick(tmp_path):
    compiler = shutil.which('clang') or shutil.which('cc')
    if not compiler:
        pytest.skip('A C compiler is required for the probe')
    (tmp_path / 'cseries.h').write_text('typedef int boolean;\n#define FALSE 0\n#define TRUE 1\n')
    source = tmp_path / 'probe.c'
    source.write_text(r'''
#include <assert.h>
#include "tick_verify.c"
static boolean setting;
boolean sim_experiment_enabled(short experiment) { assert(experiment == SIM_EXPERIMENT_VERIFY_ONCE_A_TICK); return setting; }
static int walks[32];
static int subjects[32];
static void walk(void *subject) { walks[(int *)subject - subjects]++; }
/* what a cache does: walk now unless the walk is put off */
static void verify(int index) { if (!tick_verify_put_off(&subjects[index], walk)) walk(&subjects[index]); }
static void expect(int index, int count) { assert(walks[index] == count); walks[index] = 0; }
int main(void)
{
    int index;

    /* outside a tick: at once, setting on or off */
    setting = TRUE;
    verify(0); expect(0, 1);
    /* the setting off: at once inside a tick too */
    setting = FALSE;
    tick_verify_begin(); verify(0); verify(0); expect(0, 2); tick_verify_end(); expect(0, 0);
    /* on: asked many times in a tick, made once at its end, and not again at the next */
    setting = TRUE;
    tick_verify_begin();
    for (index = 0; index < 100; index++) { verify(1); verify(2); }
    expect(1, 0); expect(2, 0);
    tick_verify_end(); expect(1, 1); expect(2, 1);
    tick_verify_begin(); tick_verify_end(); expect(1, 0); expect(2, 0);
    /* an end missed (a tick or frame begun inside another): the walks owed are made at that begin, not dropped */
    tick_verify_begin(); verify(3); tick_verify_begin(); expect(3, 1); verify(3); tick_verify_end(); expect(3, 1);
    tick_verify_end(); expect(3, 0);
    /* a subject going away: its walk is made then, once, and it is free to ask again */
    tick_verify_begin(); verify(4); tick_verify_flush(&subjects[4]); expect(4, 1);
    tick_verify_flush(&subjects[4]); expect(4, 0);
    verify(4); tick_verify_end(); expect(4, 1);
    /* no room to note another: that one walks at once, the noted ones at the end */
    tick_verify_begin();
    for (index = 0; index < 32; index++) verify(index);
    for (index = 0; index < 32; index++) assert(walks[index] == (index >= MAXIMUM_TICK_VERIFY_SUBJECTS));
    tick_verify_end();
    for (index = 0; index < 32; index++) expect(index, 1);
    /* the setting read where the tick begins holds to its end */
    tick_verify_begin(); setting = FALSE; verify(5); expect(5, 0); tick_verify_end(); expect(5, 1);
    return 0;
}
''')
    binary = tmp_path / 'probe'
    subprocess.run([compiler, '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '--include-directory=' + str(tmp_path),
                    '--include-directory=' + str(ROOT / 'port/linux/game'), str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_every_begin_in_the_game_has_its_end_on_every_path():
    """`tick_verify_begin` and `tick_verify_end` stay paired in `source/game/game.c`: each function that begins also
    ends, once each, in that order, with no way out of the function between them. The class of bug is an early
    return added between the two, which would leave walks put off and never made."""
    import re
    text = (ROOT / 'source/game/game.c').read_text(errors='surrogateescape')
    functions = re.findall(r'^(?:static )?[a-z_ \*]+?\b(\w+)\(\n(?:\t[^\n]*\n)+?\{\n(.*?)^\}', text, re.M | re.S)
    paired = {}
    for name, body in functions:
        begins = [match.start() for match in re.finditer(r'\btick_verify_begin\(\);', body)]
        ends = [match.start() for match in re.finditer(r'\btick_verify_end\(\);', body)]
        if not begins and not ends:
            continue
        assert len(begins) == 1 and len(ends) == 1 and begins[0] < ends[0], name
        between = body[begins[0]:ends[0]]
        assert not re.search(r'\b(return|goto|longjmp|exit)\b', between), name
        paired[name] = True
    # (and none outside a function body the pattern found)
    assert len(re.findall(r'\btick_verify_begin\(\);', text)) == len(paired)
    assert len(re.findall(r'\btick_verify_end\(\);', text)) == len(paired)
    assert set(paired) == {'game_tick', 'game_frame'}
