"""tag_loaded must answer NONE for a NULL name (an empty tag reference) instead of dereferencing it."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def function(source, name):
    match = re.search(r"^(?:static )?[\w *]+\b" + name + r"\(\s*[^;{]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError(f"Function not found: {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


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
    source = (ROOT / "source/cache/cache_files.c").read_text()
    code = PRELUDE + function(source, "tag_loaded") + TESTS
    with tempfile.TemporaryDirectory(prefix="tag-loaded-null-") as tmp:
        path = Path(tmp) / "test.c"
        binary = Path(tmp) / "test"
        path.write_text(code)
        subprocess.run(["xcrun", "clang", "-std=c11", "-Wall", "-Werror", "-Wno-multichar", "-fsanitize=address,undefined",
                        str(path), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
