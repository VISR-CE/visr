#!/usr/bin/env python3
"""Compare a co-op host's census with a client's under the replication contract.

Each machine logs `coop census:` lines every `debug.coop_census` seconds
(`port/linux/game/coop_census.c`): one per player record, networked object
and device, between a begin and an end line stamped with the game tick. Host
and client sample on the same ticks, so their censuses pair up by tick. This
compares every pair under the contract in `port/linux/NETCODE.md`
("Replication contract"):

- must match: the roster (each player record's absolute index, machine,
  seat, team and quit state, and its unit), every networked object's
  existence, definition, parent, dead flag and, at rest, position; a unit's
  seat, grenades, weapons, current weapon and whether something drives it;
  every carried weapon's magazines (once they have held still on both machines) and age; device positions
  and power;
- host-only: a loose weapon's magazines and age (reported, never a failure).

A difference counts only when it is there in `--persist` consecutive paired
censuses (default 2): a client learns of a change a round trip after the
host, so one census can catch it in flight. Positions are compared only for
an object at rest on both machines (it has not moved since the previous
census on either). Player identifiers are not compared: clients number their
own, so a player is matched by absolute index.

    coop_census_compare.py HOST_LOG CLIENT_LOG [--from TICK] [--to TICK] [--json OUT]

Exit status 0 when the machines agree, 1 when they disagree, 2 when there is
nothing to compare (no complete census on both).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

LINE = re.compile(r"coop census: tick (\d+) (\w+) ?(.*)$")
NONE_INDEX = "ffffffff"
POSITION_TOLERANCE = 0.05
REST_TOLERANCE = 0.002
AGE_TOLERANCE = 0.01
# (network_coop.c's DEVICE_POSITION_TOLERANCE: a client device moving within
# this of the host's is not corrected, so only a device at rest is compared)
DEVICE_TOLERANCE = 0.03
UNIT_TYPES = ("0", "1")


@dataclass
class Census:
    """One machine's census at one tick."""

    tick: int
    role: str = ""
    version: int = 0
    players: dict[int, dict] = field(default_factory=dict)
    objects: dict[str, dict] = field(default_factory=dict)
    devices: dict[str, dict] = field(default_factory=dict)
    complete: bool = False


def _fields(text: str) -> dict[str, str]:
    """`key value key value ...`, with `pos` taking three values and `rounds` four."""
    words = text.split()
    result: dict[str, str] = {}
    index = 0
    while index < len(words):
        key = words[index]
        # (a tag name, last on its line, may hold spaces)
        width = len(words) if key == "tag" else {"pos": 3, "rounds": 4, "grenades": 2, "weapons": 4, "groups": 2, "begin": 0}.get(key, 1)
        result[key] = " ".join(words[index + 1:index + 1 + width])
        index += 1 + width
    return result


def parse(path: Path) -> dict[int, Census]:
    """Every complete census in a log, by tick (a census without its end line is dropped). A tick logged
    twice (a new game in the same log restarts the clock) is ambiguous and dropped too."""
    censuses: dict[int, Census] = {}
    repeated: set[int] = set()
    current: Census | None = None
    for raw in path.read_text(errors="replace").splitlines():
        match = LINE.search(raw)
        if not match:
            continue
        tick, kind, rest = int(match[1]), match[2], match[3]
        if kind == "begin":
            role, _, version = rest.partition(" version ")
            current = Census(tick, role.strip(), int(version or 0))
            continue
        if current is None or current.tick != tick:
            continue
        if kind == "end":
            current.complete = True
            if tick in censuses:
                repeated.add(tick)
            censuses[tick] = current
            current = None
            continue
        identifier, _, body = rest.partition(" ")
        values = _fields(body)
        if kind == "player":
            current.players[int(identifier)] = values
        elif kind == "object":
            current.objects.setdefault(identifier, {}).update(values)
        elif kind in ("unit", "weapon"):
            current.objects.setdefault(identifier, {})[kind] = values
        elif kind == "device":
            current.devices[identifier] = values
    for tick in repeated:
        del censuses[tick]
    return censuses


def _position(values: dict) -> tuple[float, ...] | None:
    try:
        return tuple(float(v) for v in values["pos"].split())
    except (KeyError, ValueError):
        return None


def _distance(a: tuple[float, ...], b: tuple[float, ...]) -> float:
    return sum((x - y) ** 2 for x, y in zip(a, b)) ** 0.5


def _resting(identifier: str, census: Census, previous: Census | None) -> bool:
    if previous is None or identifier not in previous.objects:
        return False
    now, before = _position(census.objects[identifier]), _position(previous.objects[identifier])
    return now is not None and before is not None and _distance(now, before) <= REST_TOLERANCE


def _rounds_resting(identifier: str, census: Census, previous: Census | None) -> bool:
    if previous is None or identifier not in previous.objects:
        return False
    return census.objects[identifier].get("weapon", {}).get("rounds") == \
        previous.objects[identifier].get("weapon", {}).get("rounds")


def _carried(values: dict) -> bool:
    return values.get("weapon", {}).get("carried") == "1"


def differences(host: Census, client: Census, host_before: Census | None,
                client_before: Census | None) -> tuple[dict[str, str], list[str]]:
    """The contract's differences in one pair, keyed so a difference is the
    same key in the next pair; and the host-only notes."""
    found: dict[str, str] = {}
    notes: list[str] = []

    def differ(key: str, what: str) -> None:
        found[key] = what

    # the roster, by absolute index
    for index in sorted(set(host.players) | set(client.players)):
        a, b = host.players.get(index), client.players.get(index)
        if a is None or b is None:
            differ(f"player {index} exists", f"player {index}: host {'has' if a else 'lacks'} it, client {'has' if b else 'lacks'} it")
            continue
        for name in ("machine", "seat", "team", "quit", "unit"):
            if a.get(name) != b.get(name):
                differ(f"player {index} {name}", f"player {index} {name}: host {a.get(name)}, client {b.get(name)}")

    # networked objects
    for identifier in sorted(set(host.objects) | set(client.objects)):
        a, b = host.objects.get(identifier), client.objects.get(identifier)
        if a is None or b is None:
            # (a dead body or a dropped weapon the host has removed, or not yet made)
            owner = a or b
            kind = owner.get("type", "?")
            differ(f"object {identifier} exists", f"object {identifier} (type {kind}, def {owner.get('def')}): "
                   f"host {'has' if a else 'lacks'} it, client {'has' if b else 'lacks'} it")
            continue
        # (the wire carries death only for units)
        for name in ("type", "def", "parent") + (("dead",) if a.get("type") in UNIT_TYPES else ()):
            if a.get(name) != b.get(name):
                differ(f"object {identifier} {name}", f"object {identifier} {name}: host {a.get(name)}, client {b.get(name)}")
        if (a.get("parent") == NONE_INDEX and _resting(identifier, host, host_before)
                and _resting(identifier, client, client_before)):
            pa, pb = _position(a), _position(b)
            if pa and pb and _distance(pa, pb) > POSITION_TOLERANCE:
                differ(f"object {identifier} position", f"object {identifier} at rest: host {a['pos']}, client {b['pos']} "
                       f"({_distance(pa, pb):.3f} apart)")
        if "unit" in a and "unit" in b:
            ua, ub = a["unit"], b["unit"]
            names = ["player", "seat"]
            # (the host stops sending a dead unit's inventory)
            if a.get("dead") == "0":
                names += ["grenades", "current", "weapons"]
            # (whether something drives it: an actor on the host, the host's
            # control on a client; a dead unit is driven by nothing)
            if a.get("dead") == "0" and ua.get("player") == "-1":
                names.append("active")
            for name in names:
                if ua.get(name) != ub.get(name):
                    differ(f"unit {identifier} {name}", f"unit {identifier} {name}: host {ua.get(name)}, client {ub.get(name)}")
        if "weapon" in a and "weapon" in b:
            wa, wb = a["weapon"], b["weapon"]
            if wa.get("carried") != wb.get("carried"):
                differ(f"weapon {identifier} carried", f"weapon {identifier} carried: host {wa.get('carried')}, client {wb.get('carried')}")
            elif _carried(a):
                # (a weapon being fired changes every tick, and the host's
                # inventories reach a client a round trip late: counts are
                # compared once they have held still on both machines)
                if wa.get("rounds") != wb.get("rounds") and _rounds_resting(identifier, host, host_before) \
                        and _rounds_resting(identifier, client, client_before):
                    differ(f"weapon {identifier} rounds", f"weapon {identifier} carried rounds: host {wa.get('rounds')}, client {wb.get('rounds')}")
                if abs(float(wa.get("age", 0)) - float(wb.get("age", 0))) > AGE_TOLERANCE:
                    differ(f"weapon {identifier} age", f"weapon {identifier} carried age: host {wa.get('age')}, client {wb.get('age')}")
            elif wa.get("rounds") != wb.get("rounds") or wa.get("age") != wb.get("age"):
                notes.append(f"weapon {identifier} loose (host-only): host rounds {wa.get('rounds')} age {wa.get('age')}, "
                             f"client rounds {wb.get('rounds')} age {wb.get('age')}")

    # devices
    for identifier in sorted(set(host.devices) | set(client.devices)):
        a, b = host.devices.get(identifier), client.devices.get(identifier)
        if a is None or b is None:
            differ(f"device {identifier} exists", f"device {identifier}: host {'has' if a else 'lacks'} it, client {'has' if b else 'lacks'} it")
            continue
        for name in ("position", "power"):
            resting = all(before is not None and identifier in before.devices
                          and before.devices[identifier].get(name) == now.get(name)
                          for before, now in ((host_before, a), (client_before, b)))
            if resting and abs(float(a.get(name, 0)) - float(b.get(name, 0))) > DEVICE_TOLERANCE:
                differ(f"device {identifier} {name}", f"device {identifier} {name} ({a.get('tag', '?')}, groups "
                       f"{a.get('groups', '?')}): host {a.get(name)}, client {b.get(name)}")
    return found, notes


def compare(host_log: Path, client_log: Path, first: int = 0, last: int | None = None,
            persist: int = 2) -> dict:
    hosts, clients = parse(host_log), parse(client_log)
    ticks = sorted(t for t in set(hosts) & set(clients) if t >= first and (last is None or t <= last))
    report: dict = {"host": str(host_log), "client": str(client_log), "paired": len(ticks),
                    "first": ticks[0] if ticks else None, "last": ticks[-1] if ticks else None,
                    "persist": persist, "disagreements": [], "host_only": [], "transient": 0,
                    "unpaired": {role: len([t for t in set(mine) - set(theirs)
                                            if t >= first and (last is None or t <= last)])
                                 for role, mine, theirs in (("host", hosts, clients), ("client", clients, hosts))}}
    streak: dict[str, int] = {}
    reported: set[str] = set()
    previous_tick = None
    for tick in ticks:
        host_before = hosts.get(previous_tick) if previous_tick is not None else None
        client_before = clients.get(previous_tick) if previous_tick is not None else None
        found, notes = differences(hosts[tick], clients[tick], host_before, client_before)
        for key in list(streak):
            if key not in found:
                if streak[key] < persist:
                    report["transient"] += 1
                del streak[key]
        for key, what in found.items():
            streak[key] = streak.get(key, 0) + 1
            if streak[key] == persist and key not in reported:
                reported.add(key)
                report["disagreements"].append({"tick": tick, "key": key, "what": what})
        for note in notes:
            report["host_only"].append({"tick": tick, "what": note})
        previous_tick = tick
    report["agree"] = bool(ticks) and not report["disagreements"]
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("host_log", type=Path)
    parser.add_argument("client_log", type=Path)
    parser.add_argument("--from", dest="first", type=int, default=0, help="the first tick compared")
    parser.add_argument("--to", dest="last", type=int, help="the last tick compared")
    parser.add_argument("--persist", type=int, default=2,
                        help="consecutive paired censuses a difference must last to count (default 2)")
    parser.add_argument("--json", type=Path, help="write the full report here")
    args = parser.parse_args(argv)
    report = compare(args.host_log, args.client_log, args.first, args.last, args.persist)
    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + "\n")
    if not report["paired"]:
        print("coop census: no tick with a complete census on both machines")
        return 2
    print(f"coop census: {report['paired']} paired censuses, ticks {report['first']} to {report['last']}: "
          f"{'agree' if report['agree'] else 'DISAGREE'} ({len(report['disagreements'])} lasting differences, "
          f"{report['transient']} in flight, {len(report['host_only'])} host-only notes; unpaired: host "
          f"{report['unpaired']['host']}, client {report['unpaired']['client']})")
    for item in report["disagreements"][:40]:
        print(f"  tick {item['tick']}: {item['what']}")
    if len(report["disagreements"]) > 40:
        print(f"  ... {len(report['disagreements']) - 40} more")
    shown = {}
    for item in report["host_only"]:
        shown[item["what"].split(":")[0]] = item
    for item in list(shown.values())[:10]:
        print(f"  host-only, tick {item['tick']}: {item['what']}")
    return 0 if report["agree"] else 1


if __name__ == "__main__":
    sys.exit(main())
