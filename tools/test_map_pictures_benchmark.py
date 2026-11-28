"""The map picture import owns map loading before a configured benchmark starts."""
from pathlib import Path
import subprocess

from probekit import cc

ROOT = Path(__file__).resolve().parents[1]


def test_benchmark_waits_for_map_picture_import(tmp_path):
    source = tmp_path / "benchmark-import.c"
    source.write_text(r'''
#define main benchmark_startup_probe_main
#include "benchmark_startup_probe.c"
#undef main
int main(void)
{
    reset();
    map_pictures_import_pending = 1;
    CHECK(frame(0) == -1);
    CHECK(frame(1000000) == -1);
    CHECK(suite.state == suite_unasked);
    CHECK(starts == 0 && requests == 0 && loads == 0 && suite_suspends == 0);
    CHECK(readiness_reads == 0 && timeline_reads == 0);
    map_pictures_import_pending = 0;
    CHECK(frame(1000001) == -1);
    CHECK(starts == 1 && suite_suspends == 1);
    CHECK(suite.state != suite_unasked && suite.state != suite_off);
    CHECK(frame(1000001 + MENU_SETTLE_MILLISECONDS) == -1);
    CHECK(suite.state == suite_next && requests == 0 && loads == 0);
    CHECK(frame(1000002 + MENU_SETTLE_MILLISECONDS) == -1);
    CHECK(requests == 1 && loads == 1);
    return failures != 0;
}
''')
    binary = tmp_path / "benchmark-import"
    cc.build([source], output=binary, std="c99", defines=["_DEFAULT_SOURCE"],
             includes=[ROOT / "port/ios/tests", ROOT / "port/linux/src"],
             options=["-iquote", ROOT / "port/linux/include"], port_stubs=True)
    subprocess.run([str(binary)], check=True)
