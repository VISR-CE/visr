"""tag_loaded must answer NONE for a NULL name (an empty tag reference) instead of dereferencing it."""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from probekit import source as probe_source  # noqa: E402


PRELUDE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define NONE (-1)
#define match_assert(file, line, condition) assert(condition)
#define _stricmp strcasecmp /* the port's version dereferences both strings, as this does */
static struct { int tags_loaded; } cache_file_globals;
static struct tag_instance { long group_tag; const char *name; long tag_index; } *global_tag_instances;
static long global_tag_count;
'''

TESTS = r'''
int main(void) {
    struct tag_instance tags[] = {{'unit', "vehicles\\warthog\\warthog", 7}, {'hud ', "ui\\hud\\seat", 9}};
    global_tag_instances = tags;
    global_tag_count = 2;
    cache_file_globals.tags_loaded = 1;
    assert(tag_loaded('unit', "VEHICLES\\warthog\\warthog") == 7);
    assert(tag_loaded('unit', "ui\\hud\\seat") == NONE);
    assert(tag_loaded('unit', NULL) == NONE);
    assert(tag_loaded('hud ', NULL) == NONE);
    cache_file_globals.tags_loaded = 0;
    assert(tag_loaded('unit', NULL) == NONE);
    puts("PASS: tag_loaded answers NONE for a NULL name");
    return 0;
}
'''


def main():
    code = PRELUDE + probe_source.function("source/cache/cache_files.c", "tag_loaded") + TESTS
    with tempfile.TemporaryDirectory(prefix="tag-loaded-null-") as tmp:
        path = Path(tmp) / "test.c"
        binary = Path(tmp) / "test"
        path.write_text(code)
        subprocess.run(["xcrun", "clang", "-std=c11", "-Wall", "-Werror", "-Wno-multichar", "-fsanitize=address,undefined",
                        str(path), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
