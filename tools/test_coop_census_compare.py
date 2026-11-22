"""tools/coop_census_compare.py: pairing, the contract's fields, persistence,
and the census format the game writes (port/linux/game/coop_census.c)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import coop_census_compare as census  # noqa: E402

NONE = "ffffffff"


def census_lines(tick: int, role: str, players=None, objects=None, devices=None) -> list[str]:
    """A census as the game logs it; objects are (id, fields) with optional unit/weapon dicts."""
    lines = [f"coop census: tick {tick} begin {role} version 38"]
    for index, values in (players or {}).items():
        row = {"id": f"{0xe0000000 + index:08x}", "machine": "0", "seat": "0/0", "team": "0", "quit": "0", "unit": NONE}
        row.update(values)
        lines.append(f"coop census: tick {tick} player {index} " + " ".join(f"{k} {v}" for k, v in row.items()))
    for identifier, values in (objects or {}).items():
        row = {"type": "0", "def": "e1230001", "parent": NONE, "dead": "0", "map": "1", "pos": "1.000 2.000 3.000"}
        row.update({k: v for k, v in values.items() if k not in ("unit", "weapon")})
        lines.append(f"coop census: tick {tick} object {identifier} " + " ".join(f"{k} {v}" for k, v in row.items()))
        if "unit" in values:
            unit = {"player": "-1", "seat": "-1", "active": "1", "grenades": "0 0", "current": "0",
                    "weapons": f"{NONE} {NONE} {NONE} {NONE}"}
            unit.update(values["unit"])
            lines.append(f"coop census: tick {tick} unit {identifier} " + " ".join(f"{k} {v}" for k, v in unit.items()))
        if "weapon" in values:
            weapon = {"carried": "0", "rounds": "12 12 0 0", "age": "0.000"}
            weapon.update(values["weapon"])
            lines.append(f"coop census: tick {tick} weapon {identifier} " + " ".join(f"{k} {v}" for k, v in weapon.items()))
    for identifier, values in (devices or {}).items():
        lines.append(f"coop census: tick {tick} device {identifier} position {values[0]:.3f} power {values[1]:.3f} groups 3 -1 tag holo control")
    lines.append(f"coop census: tick {tick} end objects {len(objects or {})} devices {len(devices or {})}")
    return lines


def write(tmp_path: Path, name: str, censuses: list[list[str]]) -> Path:
    path = tmp_path / name
    path.write_text("\n".join("[12:00:00] " + line for lines in censuses for line in lines) + "\n")
    return path


def run(tmp_path, host_censuses, client_censuses, **options):
    return census.compare(write(tmp_path, "host.log", host_censuses), write(tmp_path, "client.log", client_censuses), **options)


WORLD = {
    "e0010004": {"type": "0", "unit": {"player": "0", "weapons": f"e0020005 {NONE} {NONE} {NONE}"}},
    "e0020005": {"type": "2", "parent": "e0010004", "weapon": {"carried": "1", "rounds": "60 32 0 0", "age": "0.000"}},
    "e0030006": {"type": "0", "unit": {"active": "1"}},
}


def test_agreeing_machines_agree(tmp_path):
    host = [census_lines(t, "host", {0: {"unit": "e0010004"}}, WORLD, {"e0040007": (1.0, 1.0)}) for t in (150, 300, 450)]
    client = [census_lines(t, "client", {0: {"unit": "e0010004", "id": "00010000"}}, WORLD, {"e0040007": (1.0, 1.0)})
              for t in (150, 300, 450)]
    report = run(tmp_path, host, client)
    assert report["paired"] == 3 and report["agree"], report


def test_lasting_difference_is_reported_once(tmp_path):
    stale = dict(WORLD, e0050008={"type": "0", "dead": "1"})
    host = [census_lines(t, "host", {}, WORLD) for t in (150, 300, 450)]
    client = [census_lines(t, "client", {}, stale) for t in (150, 300, 450)]
    report = run(tmp_path, host, client)
    assert not report["agree"]
    assert [d["key"] for d in report["disagreements"]] == ["object e0050008 exists"]
    assert report["disagreements"][0]["tick"] == 300


def test_difference_in_flight_is_not_a_disagreement(tmp_path):
    late = dict(WORLD)
    del late["e0030006"]
    host = [census_lines(t, "host", {}, WORLD) for t in (150, 300, 450)]
    client = [census_lines(150, "client", {}, late), census_lines(300, "client", {}, WORLD), census_lines(450, "client", {}, WORLD)]
    report = run(tmp_path, host, client)
    assert report["agree"] and report["transient"] == 1, report


def test_roster_compared_by_absolute_index(tmp_path):
    host = [census_lines(t, "host", {0: {}, 1: {"seat": "1/0", "quit": "0"}}) for t in (150, 300)]
    client = [census_lines(t, "client", {0: {"id": "12340000"}, 1: {"seat": "-1/-1", "quit": "1", "id": "12350001"}})
              for t in (150, 300)]
    keys = {d["key"] for d in run(tmp_path, host, client)["disagreements"]}
    assert keys == {"player 1 seat", "player 1 quit"}


def test_uncontrolled_actor_unit_disagrees(tmp_path):
    lost = dict(WORLD, e0030006={"type": "0", "unit": {"active": "0"}})
    host = [census_lines(t, "host", {}, WORLD) for t in (150, 300)]
    client = [census_lines(t, "client", {}, lost) for t in (150, 300)]
    assert [d["key"] for d in run(tmp_path, host, client)["disagreements"]] == ["unit e0030006 active"]


def test_carried_ammunition_must_match_and_loose_is_host_only(tmp_path):
    loose = {"e0060009": {"type": "2", "weapon": {"carried": "0", "rounds": "24 12 0 0"}}}
    carried_wrong = dict(WORLD, e0020005={"type": "2", "parent": "e0010004",
                                          "weapon": {"carried": "1", "rounds": "72 32 0 0"}})
    host = [census_lines(t, "host", {}, {**WORLD, **loose}) for t in (150, 300, 450)]
    client = [census_lines(t, "client", {}, {**carried_wrong, "e0060009": {"type": "2", "weapon": {"rounds": "72 12 0 0"}}})
              for t in (150, 300, 450)]
    report = run(tmp_path, host, client)
    assert [d["key"] for d in report["disagreements"]] == ["weapon e0020005 rounds"]
    assert report["host_only"] and all("e0060009" in n["what"] for n in report["host_only"])


def test_position_compared_only_at_rest(tmp_path):
    moving = [census_lines(t, "host", {}, {"e0070001": {"pos": f"{t / 10:.3f} 0.000 0.000"}}) for t in (150, 300, 450)]
    behind = [census_lines(t, "client", {}, {"e0070001": {"pos": f"{t / 10 - 1:.3f} 0.000 0.000"}}) for t in (150, 300, 450)]
    assert run(tmp_path, moving, behind)["agree"]
    resting = [census_lines(t, "host", {}, {"e0070001": {"pos": "5.000 0.000 0.000"}}) for t in (150, 300, 450)]
    apart = [census_lines(t, "client", {}, {"e0070001": {"pos": "6.000 0.000 0.000"}}) for t in (150, 300, 450)]
    assert [d["key"] for d in run(tmp_path, resting, apart)["disagreements"]] == ["object e0070001 position"]


def test_incomplete_census_is_not_paired(tmp_path):
    host = [census_lines(150, "host", {}, WORLD)[:-1], census_lines(300, "host", {}, WORLD)]
    client = [census_lines(t, "client", {}, WORLD) for t in (150, 300)]
    assert run(tmp_path, host, client)["paired"] == 1


def test_device_difference(tmp_path):
    host = [census_lines(t, "host", devices={"e0040007": (1.0, 1.0)}) for t in (150, 300, 450)]
    client = [census_lines(t, "client", devices={"e0040007": (0.0, 1.0)}) for t in (150, 300, 450)]
    report = run(tmp_path, host, client)
    assert [d["key"] for d in report["disagreements"]] == ["device e0040007 position"]
    assert "(holo control, groups 3 -1)" in report["disagreements"][0]["what"]


def test_moving_device_within_the_wire_tolerance_agrees(tmp_path):
    # (a moving device is corrected only beyond 0.03, and only a device at rest is compared)
    host = [census_lines(t, "host", devices={"e0040007": (p, 1.0)}) for t, p in ((150, 0.1), (300, 0.3), (450, 0.5))]
    client = [census_lines(t, "client", devices={"e0040007": (p, 1.0)}) for t, p in ((150, 0.08), (300, 0.28), (450, 0.48))]
    assert run(tmp_path, host, client)["agree"]


def test_dead_unit_inventory_is_not_compared(tmp_path):
    # (the host stops sending a dead unit's inventory)
    host = [census_lines(t, "host", objects={"e0030006": {"dead": "1", "unit": {"grenades": "2 0"}}}) for t in (150, 300, 450)]
    client = [census_lines(t, "client", objects={"e0030006": {"dead": "1", "unit": {"grenades": "0 0"}}}) for t in (150, 300, 450)]
    assert run(tmp_path, host, client)["agree"]


def test_death_is_compared_only_for_units(tmp_path):
    # (the wire carries death only for bipeds and vehicles)
    host = [census_lines(t, "host", objects={"e0050008": {"type": "2", "dead": "1"}, "e0030006": {"dead": "1", "unit": {}}})
            for t in (150, 300, 450)]
    client = [census_lines(t, "client", objects={"e0050008": {"type": "2", "dead": "0"}, "e0030006": {"dead": "0", "unit": {}}})
              for t in (150, 300, 450)]
    assert [d["key"] for d in run(tmp_path, host, client)["disagreements"]] == ["object e0030006 dead"]


def test_repeated_tick_is_not_paired(tmp_path):
    # (a second game in the same log restarts the clock: tick 300 is ambiguous)
    host = [census_lines(150, "host"), census_lines(300, "host"), census_lines(300, "host", objects={"e0010004": {}})]
    client = [census_lines(150, "client"), census_lines(300, "client")]
    report = run(tmp_path, host, client)
    assert report["paired"] == 1 and report["unpaired"] == {"host": 0, "client": 1}


def test_unpaired_censuses_are_counted(tmp_path):
    host = [census_lines(t, "host") for t in (150, 300, 450)]
    client = [census_lines(t, "client") for t in (150, 450)]
    report = run(tmp_path, host, client)
    assert report["paired"] == 2 and report["unpaired"] == {"host": 1, "client": 0}


def _c_format_to_python(fmt: str) -> str:
    return re.sub(r"%(0?\d*)(?:\.(\d+))?(l?)([dxufs])",
                  lambda m: "{:" + (m[1] or "") + ("." + m[2] if m[2] else "") + {"d": "d", "u": "d", "x": "x", "f": "f", "s": "s"}[m[4]] + "}"
                  if m[4] != "f" else "{:." + (m[2] or "6") + "f}", fmt)


def test_game_format_parses(tmp_path):
    """Every census line coop_census.c writes, rendered with sample values, parses into the
    fields the comparer reads: a format change that drops or renames one fails here."""
    source = (ROOT / "port/linux/game/coop_census.c").read_text()
    formats = re.findall(r'platform_log\("(coop census: [^"]*)"', source)
    kinds = {re.match(r"coop census: tick %ld (\w+)", f)[1]: f for f in formats}
    assert set(kinds) == {"begin", "player", "object", "unit", "weapon", "device", "name", "end"}, kinds

    def render(fmt: str, values: list) -> str:
        return _c_format_to_python(fmt).format(*values)

    lines = [
        render(kinds["begin"], [300, "host", 38]),
        render(kinds["player"], [300, 0, 0xe0000000, 0, 0, 1, 2, 0, 0xe0010004]),
        render(kinds["object"], [300, 0xe0010004, 0, 0xe1230001, 0xffffffff, 0, 1, 1.0, 2.0, 3.0]),
        render(kinds["unit"], [300, 0xe0010004, 0, -1, 1, 2, 1, 0, 0xe0020005, 0xffffffff, 0xffffffff, 0xffffffff]),
        render(kinds["object"], [300, 0xe0020005, 2, 0xe1230002, 0xe0010004, 0, 0, 1.0, 2.0, 3.0]),
        render(kinds["weapon"], [300, 0xe0020005, 1, 60, 32, 0, 0, 0.0]),
        render(kinds["device"], [300, 0xe0040007, 1.0, 0.5, 3, -1, "door"]),
        render(kinds["end"], [300, 2, 1]),
    ]
    path = tmp_path / "game.log"
    path.write_text("\n".join(lines) + "\n")
    parsed = census.parse(path)[300]
    assert parsed.complete and parsed.role == "host" and parsed.version == 38
    assert parsed.players[0]["seat"] == "0/1" and parsed.players[0]["unit"] == "e0010004"
    unit = parsed.objects["e0010004"]
    assert unit["pos"] == "1.000 2.000 3.000" and unit["unit"]["active"] == "1"
    assert unit["unit"]["grenades"] == "2 1" and unit["unit"]["weapons"].split()[0] == "e0020005"
    weapon = parsed.objects["e0020005"]
    assert weapon["parent"] == "e0010004" and weapon["weapon"]["rounds"] == "60 32 0 0"
    assert parsed.devices["e0040007"] == {"position": "1.000", "power": "0.500", "groups": "3 -1", "tag": "door"}


def test_carried_rounds_while_firing_are_in_flight(tmp_path):
    """a weapon whose count changes between censuses is being fired: not compared until it holds still"""
    def world(rounds):
        return dict(WORLD, e0020005={"type": "2", "parent": "e0010004", "weapon": {"carried": "1", "rounds": rounds}})
    host = [census_lines(t, "host", {}, world(f"180 {50 - t // 150} 0 0")) for t in (150, 300, 450, 600)]
    client = [census_lines(t, "client", {}, world(f"180 {52 - t // 150} 0 0")) for t in (150, 300, 450, 600)]
    assert run(tmp_path, host, client)["agree"]
    host_still = host[:2] + [census_lines(t, "host", {}, world("180 40 0 0")) for t in (450, 600, 750)]
    client_still = client[:2] + [census_lines(t, "client", {}, world("180 42 0 0")) for t in (450, 600, 750)]
    assert [d["key"] for d in run(tmp_path, host_still, client_still)["disagreements"]] == ["weapon e0020005 rounds"]
