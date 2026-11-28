"""A projectile whose owner object was deleted must not reach a checked lookup in the damage path."""
from pathlib import Path
import subprocess
import tempfile

from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]


PROBE = r"""
#include <stdio.h>
#include <stdlib.h>
#define NONE (-1)
struct damage_data { long owner_player_index, owner_object_index; };
struct object_datum { struct { long parent_object_index; } object; };
struct unit_datum { int dummy; };
static struct object_datum live = { { NONE } };
/* only index 1 is alive; any other index is a deleted datum */
static struct object_datum *object_try_and_get(long i) { return i == 1 ? &live : NULL; }
static struct unit_datum *unit_try_and_get(long i) { (void)i; return NULL; }
static void *player_try_and_get(long i) { (void)i; return (void *)1; }
static long player_index_from_unit_index(long i) { return i; }
PRODUCTION
int main(void)
{
	struct damage_data d = { NONE, 7 };
	damage_data_validate_owner(&d);
	if (d.owner_object_index != NONE) { puts("stale owner kept"); return 1; }
	if (get_player_index_from_object_or_parents(7) != NONE) return 1;
	d.owner_object_index = 1;
	damage_data_validate_owner(&d);
	if (d.owner_object_index != 1) { puts("live owner dropped"); return 1; }
	puts("ok");
	return 0;
}
"""


def test_owner_gone():
    damage = ROOT / "source/objects/damage.c"
    body = probe_source.function(damage, "damage_data_validate_owner") + "\n" + \
        probe_source.function(damage, "get_player_index_from_object_or_parents")
    code = PROBE.replace("PRODUCTION", body)
    with tempfile.TemporaryDirectory() as tmp:
        src, exe = Path(tmp) / "p.c", Path(tmp) / "p"
        src.write_text(code)
        subprocess.run(["cc", "-Wall", "-o", str(exe), str(src)], check=True)
        out = subprocess.run([str(exe)], capture_output=True, text=True)
        assert out.returncode == 0 and "ok" in out.stdout, out.stdout
