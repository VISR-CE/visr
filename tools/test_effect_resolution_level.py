"""`effect_resolution_level` (port/linux/src/d3d8_device.c) reads display.effect_resolution's three values, and the old
boolean's "true" and "false" from the environment or a --set as "half" and "xbox"."""
import shutil
import subprocess
from pathlib import Path

import pytest

from probekit import cc, source as probe_source

ROOT = Path(__file__).resolve().parents[1]


def _function():
    return probe_source.function(ROOT / "port/linux/src/d3d8_device.c", "effect_resolution_level") + "\n"


@pytest.mark.skipif(not shutil.which("cc"), reason="needs a C compiler")
def test_effect_resolution_levels(tmp_path):
    program = tmp_path / "level.c"
    program.write_text(
        "#include <stdio.h>\n#include <string.h>\n#include <stdarg.h>\n"
        "static int logged;\n"
        "/* fake: counts the warnings the level function logs */\n"
        "static void platform_log(const char *format, ...) { (void)format; logged++; }\n"
        + _function() +
        "int main(void) {\n"
        "    const char *values[] = { \"xbox\", \"half\", \"full\", \"true\", \"false\", \"bogus\" };\n"
        "    for (int i = 0; i < 6; i++) {\n"
        "        logged = 0;\n"
        "        int level = effect_resolution_level(values[i]);\n"
        "        printf(\"%s %d %d\\n\", values[i], level, logged);\n"
        "    }\n"
        "    return 0;\n}\n")
    binary = tmp_path / "level"
    cc.build([program], output=binary, extra_warnings=True)
    lines = subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout.split("\n")
    assert lines[:6] == ["xbox 0 0", "half 1 0", "full 2 0", "true 1 1", "false 0 1", "bogus 1 1"]
