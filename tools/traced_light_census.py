#!/usr/bin/env python3
"""List every dynamic light an Xbox level map can make, by what owns it (stdlib only).

    python3 tools/traced_light_census.py MAP... --output tools/traced_light_stock.tsv

Blocked lights (display.traced_lights) trace a light by its owner's object type
(port/linux/game/light_visibility.h): a unit, weapon, item or projectile, or no
object at all, is traced; scenery and devices are not, because the level's
designers placed those lights against a world where light passes through walls.
This reads which tags can own each light:

- an object definition that attaches the light (its attachments block);
- an effect with a light part, through every tag that refers to the effect. An
  effect light belongs to the effect's object, so an effect a scenery or device
  definition refers to is owned by that object, one a weapon or projectile refers
  to by that weapon or projectile; one a scenario refers to is a scripted effect
  (owner none, or the object the script names).

Attached lights reach the dynamic light passes only with the light tag's dynamic
flag; effect lights always do. A row per light tag, owner kind and owner group,
with the maps it is on. The checked-in table is tools/traced_light_stock.tsv;
tools/test_traced_shadows.py checks the C rule against it.
"""
from __future__ import annotations

import argparse
import bisect
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ui_map_tags import MapReader  # noqa: E402

# source/objects/object_types.h, in order
OBJECT_GROUPS = ("bipd", "vehi", "weap", "eqip", "garb", "proj", "scen", "mach", "ctrl", "lifi", "plac", "ssce")
OBJECT_ATTACHMENTS = 0x140  # _object_definition.attachments (object_definitions.h)
ATTACHMENT_SIZE = 0x48  # object_attachment_definition
EFFECT_EVENTS, EVENT_SIZE, EVENT_PARTS = 0x34, 0x44, 0x2C  # effect_definitions.h
PART_SIZE, PART_REFERENCE = 0x68, 0x18
LIGHT_DYNAMIC = 1  # _light_definition_dynamic_bit


def group_bytes(group: str) -> bytes:
    return group[::-1].encode("latin-1")


class Census:
    def __init__(self, reader: MapReader):
        self.reader = reader
        self.by_offset = []
        for tag in reader.tags.values():
            try:
                tag["offset"] = reader.offset(tag["base"], 4)
            except ValueError:
                continue
            self.by_offset.append((tag["offset"], tag["id"]))
        self.by_offset.sort()
        self.starts = [offset for offset, _ in self.by_offset]

    def containing(self, offset: int):
        index = bisect.bisect_right(self.starts, offset) - 1
        return self.reader.tags[self.by_offset[index][1]] if index >= 0 else None

    def referrers(self, tag):
        """Tags whose data holds a tag reference to this tag."""
        data, needle, group = self.reader.data, struct.pack("<I", tag["id"]), group_bytes(tag["group"])
        found, position = {}, self.reader.start
        while True:
            position = data.find(needle, position, self.reader.end)
            if position < 0:
                return list(found.values())
            if position % 4 == 0 and data[position - 12:position - 8] == group:
                owner = self.containing(position)
                if owner is not None and owner["id"] != tag["id"]:
                    found[owner["id"]] = owner
            position += 4

    def light_flags(self, tag) -> int:
        return self.reader.unpack("<I", tag["offset"])[0]

    def attached_lights(self, tag):
        reader = self.reader
        for offset in reader.block(tag["offset"] + OBJECT_ATTACHMENTS, ATTACHMENT_SIZE):
            group, = reader.unpack("<4s", offset)
            tag_id, = reader.unpack("<I", offset + 12)
            if group == group_bytes("ligh") and tag_id in reader.tags:
                yield reader.tags[tag_id]

    def effect_lights(self, tag):
        reader = self.reader
        for event in reader.block(tag["offset"] + EFFECT_EVENTS, EVENT_SIZE):
            for part in reader.block(event + EVENT_PARTS, PART_SIZE):
                group, = reader.unpack("<4s", part + PART_REFERENCE)
                tag_id, = reader.unpack("<I", part + PART_REFERENCE + 12)
                if group == group_bytes("ligh") and tag_id in reader.tags:
                    yield reader.tags[tag_id]

    def effect_owners(self, effect, seen=None):
        """(kind, group) for every way an effect can be made: an object group owns it,
        a scenario scripts it, anything else (damage, material, globals) makes it on no object."""
        seen = seen if seen is not None else set()
        if effect["id"] in seen:
            return set()
        seen.add(effect["id"])
        owners = set()
        for referrer in self.referrers(effect):
            group = referrer["group"]
            if group in OBJECT_GROUPS:
                owners.add(("effect", group))
            elif group == "effe":
                owners |= self.effect_owners(referrer, seen)
            elif group == "scnr":
                owners.add(("scripted effect", "scnr"))
            else:
                owners.add(("effect", group))
        return owners or {("effect", "unreferenced")}

    def rows(self):
        for tag in self.reader.tags.values():
            if "offset" not in tag:
                continue
            if tag["group"] in OBJECT_GROUPS:
                for light in self.attached_lights(tag):
                    if self.light_flags(light) & LIGHT_DYNAMIC:
                        yield light["path"], "attached", tag["group"]
            elif tag["group"] == "effe":
                lights = {light["id"]: light for light in self.effect_lights(tag)}
                if lights:
                    owners = self.effect_owners(tag)
                    for light in lights.values():
                        for kind, group in owners:
                            yield light["path"], kind, group


def traced(kind: str, group: str) -> str:
    """The rule in light_visibility.h, for the table: scenery and devices never trace."""
    if group in ("scen", "mach", "ctrl", "lifi", "plac", "ssce"):
        return "no"
    if kind == "scripted effect":
        return "scripted"
    return "yes"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("maps", nargs="+", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    table: dict[tuple[str, str, str], set[str]] = {}
    for path in args.maps:
        name = path.name.split("-")[0].removesuffix(".map")
        census = Census(MapReader(path.read_bytes()))
        for light, kind, group in census.rows():
            table.setdefault((light, kind, group), set()).add(name)
    lines = ["light\towner_kind\towner_group\ttraced\tmaps"]
    for (light, kind, group), maps in sorted(table.items()):
        lines.append(f"{light}\t{kind}\t{group}\t{traced(kind, group)}\t{','.join(sorted(maps))}")
    text = "\n".join(lines) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
