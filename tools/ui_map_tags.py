#!/usr/bin/env python3
"""Inspect an Xbox ui.map without running the game (stdlib only).

    python3 tools/ui_map_tags.py /path/to/ui.map --output tags.json
    python3 tools/ui_map_tags.py --check-menus port/assets/menus/xbox
    python3 tools/ui_map_tags.py /path/to/ui.map --check-menus port/assets/menus/xbox

JSON contains a tag list (id, group, path) and widgets keyed by tag path. Bounds
are [left, top, right, bottom]; child x/y are offsets, not absolute positions.
Handler functions and game-data inputs retain their numeric engine indexes.
Menu checks resolve XML names, require PNG fallbacks relative to the menus
folder, and forbid map widget dependencies in XML screens. With a map they
also resolve map art, open targets and patch targets against its tag groups.
Layouts follow port/linux/game/menu_tags.c, as in the original widget probe.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys
from xml.etree import ElementTree
import zlib

BASE = 0x803A6000
HEADER_SIZE = 0x800
TYPES = ("container", "text_box", "spinner_list", "column_list")
EVENTS = ("a", "b", "x", "y", "black", "white", "left_trigger", "right_trigger",
          "up", "down", "left", "right", "start", "back", "left_thumb", "right_thumb",
          "stick_up", "stick_down", "stick_left", "stick_right", "right_stick_up",
          "right_stick_down", "right_stick_left", "right_stick_right", "created", "deleted")
HANDLER_FLAGS = ("close_current", "close_other", "close_all", "open", "reload_self", "reload_other",
                 "give_focus", "run_function", "replace_self", "go_back", "run_script", "branch_on_failure")


class MapReader:
    """Bounds-check the Xbox tag cache before translating its guest pointers."""

    def __init__(self, raw):
        if len(raw) < HEADER_SIZE or raw[:4] != b"daeh" or raw[0x7FC:0x800] != b"toof":
            raise ValueError("not an Xbox cache header")
        version, file_size = struct.unpack_from("<II", raw, 4)
        if version != 5:
            raise ValueError(f"unsupported cache version {version}; expected Xbox version 5")
        if not HEADER_SIZE <= file_size <= 0x18000000:
            raise ValueError("invalid cache file size")
        if len(raw) < file_size:
            try:
                inflater = zlib.decompressobj()
                payload = inflater.decompress(raw[HEADER_SIZE:], file_size - HEADER_SIZE + 1)
            except zlib.error as error:
                raise ValueError(f"cannot decompress Xbox cache: {error}") from error
            if not inflater.eof or len(payload) != file_size - HEADER_SIZE:
                raise ValueError("decompressed cache size does not match its header")
            raw = raw[:HEADER_SIZE] + payload
        self.data = raw[:file_size]
        self.start, size = struct.unpack_from("<II", raw, 0x10)
        self.end = self.start + size
        if self.start < HEADER_SIZE or size < 16 or self.end > len(self.data):
            raise ValueError("tag data is outside the cache file")
        array, _, _, count = self.unpack("<IIII", self.start)
        table = self.offset(array, count * 32)
        self.tags = {}
        for index in range(count):
            group, _, _, tag_id, name, base, _, _ = self.unpack("<4s4s4sIIIII", table + index * 32)
            if tag_id in self.tags:
                raise ValueError(f"duplicate tag id {tag_id:#x}")
            self.tags[tag_id] = {"id": tag_id, "group": group[::-1].decode("latin-1"),
                                 "path": self.cstring(name), "base": base}

    def offset(self, address, size):
        offset = address - BASE + self.start
        self.check(offset, size)
        return offset

    def check(self, offset, size):
        if size < 0 or offset < self.start or offset > self.end or size > self.end - offset:
            raise ValueError(f"tag data range {offset:#x}+{size:#x} is outside the tag cache")

    def unpack(self, fmt, offset):
        self.check(offset, struct.calcsize(fmt))
        return struct.unpack_from(fmt, self.data, offset)

    def cstring(self, address):
        if not address:
            return ""
        start = self.offset(address, 1)
        end = self.data.find(b"\0", start, self.end)
        if end < 0:
            raise ValueError("unterminated tag path")
        return self.data[start:end].decode("latin-1")

    def reference(self, offset):
        tag_id, = self.unpack("<I", offset + 12)
        if tag_id == 0xFFFFFFFF:
            return ""
        if tag_id not in self.tags:
            raise ValueError(f"unknown tag reference {tag_id:#x} at {offset:#x}")
        return self.tags[tag_id]["path"]

    def block(self, offset, stride):
        count, address, _ = self.unpack("<iII", offset)
        if count < 0:
            raise ValueError(f"negative tag block count at {offset:#x}")
        if not count:
            return range(0)
        start = self.offset(address, count * stride)
        return range(start, start + count * stride, stride)

    def string_lists(self):
        result = {}
        for tag in self.tags.values():
            if tag["group"] != "ustr":
                continue
            strings = []
            for offset in self.block(self.offset(tag["base"], 12), 20):
                size, _, _, address, _ = self.unpack("<iIIII", offset)
                if size < 0 or size % 2:
                    raise ValueError("invalid UTF-16 string size")
                start = self.offset(address, size) if size else 0
                strings.append(self.data[start:start + size].decode("utf-16-le").rstrip("\0"))
            result[tag["path"]] = strings
        return result

    def inventory(self):
        strings = self.string_lists()
        widgets = {}
        for tag in self.tags.values():
            if tag["group"] != "DeLa":
                continue
            w = self.offset(tag["base"], 0x3EC)
            kind, controller = self.unpack("<hh", w)
            top, left, bottom, right = self.unpack("<hhhh", w + 0x24)
            flags, = self.unpack("<I", w + 0x2C)
            text_list = self.reference(w + 0xEC)
            text = strings.get(text_list, [])
            string_index, = self.unpack("<h", w + 0x12E)
            list_flags, = self.unpack("<I", w + 0x150)
            handlers = []
            for offset in self.block(w + 0x54, 0x48):
                hflags, event, function = self.unpack("<Ihh", offset)
                handlers.append({
                    "event": EVENTS[event] if 0 <= event < len(EVENTS) else event,
                    "function": function,
                    "flags": [flag for bit, flag in enumerate(HANDLER_FLAGS) if hflags >> bit & 1],
                    "widget": self.reference(offset + 8),
                    "script": self.data[offset + 0x28:offset + 0x48].split(b"\0", 1)[0].decode("latin-1"),
                })
            children = []
            for offset in self.block(w + 0x3E0, 0x50):
                y, x = self.unpack("<hh", offset + 0x36)
                children.append({"widget": self.reference(offset), "x": x, "y": y})
            if tag["path"] in widgets:
                raise ValueError(f"duplicate widget path {tag['path']}")
            widgets[tag["path"]] = {
                "type": TYPES[kind] if 0 <= kind < len(TYPES) else kind,
                "controller": controller, "bounds": [left, top, right, bottom],
                "flags": hex(flags), "list_flags": hex(list_flags),
                "bitmap": self.reference(w + 0x38), "font": self.reference(w + 0xFC),
                "header_bitmap": self.reference(w + 0x154), "footer_bitmap": self.reference(w + 0x164),
                "text_list": text_list, "string_index": string_index,
                "text": text[string_index] if 0 <= string_index < len(text) else
                        (text if kind != 1 and text else ""),
                "game_data_inputs": [self.unpack("<h", at)[0] for at in self.block(w + 0x48, 0x24)],
                "handlers": handlers, "children": children,
                "conditionals": [self.reference(at) for at in self.block(w + 0x2D4, 0x50)],
                "description": self.reference(w + 0x1A4),
            }
        return {"tags": [{key: tag[key] for key in ("id", "group", "path")} for tag in self.tags.values()],
                "widgets": widgets}


def read_map(path):
    """Return the tag inventory and decoded widget definitions of an Xbox map."""
    return MapReader(Path(path).read_bytes()).inventory()


def check_menus(folder, assets=None, inventory=None):
    """Return menu-reference errors; map-dependent checks are skipped without inventory."""
    folder = Path(folder)
    assets = Path(assets) if assets is not None else folder.parent
    files = sorted(folder.glob("*.xml"))
    if not files:
        return [f"{folder}: no menu XML files"]
    documents, errors = [], []
    for path in files:
        try:
            documents.append((path, ElementTree.parse(path).getroot()))
        except (OSError, ElementTree.ParseError) as error:
            errors.append(f"{path}: {error}")
    definitions = {group: set() for group in ("DeLa", "bitm", "ustr")}
    for _, root in documents:
        for element in root.iter():
            group = {"widget": "DeLa", "bitmap": "bitm", "strings": "ustr"}.get(element.tag)
            if group and element.get("name"):
                definitions[group].add(element.get("name"))
    tags = {(tag["group"], tag["path"]) for tag in inventory["tags"]} if inventory is not None else None

    def resolve(path, value, group, label):
        if not value:
            return
        if value in definitions.get(group, set()):
            return
        if "\\" not in value or tags is not None and (group, value) not in tags:
            errors.append(f"{path}: {label}={value} does not resolve to {group}")

    def resolve_map(path, value, group, label):
        if tags is not None and (group, value) not in tags:
            errors.append(f"{path}: {label}={value} does not resolve to a {group} map tag")

    def visit(path, element, patch=False, row_names=None):
        patch = patch or element.tag == "patch"
        if element.tag == "patch":
            row_names = set()
        if row_names is None:
            row_names = set()
        for key, value in element.attrib.items():
            if element.tag == "frame" and key == "map":
                resolve_map(path, value, "bitm", key)
                png = element.get("png")
                if not png or not (assets / png).is_file():
                    errors.append(f"{path}: map frame {value!r} needs an existing png fallback: {png!r}")
            elif key == "open":
                resolve(path, value, "DeLa", key)
            elif patch and key in ("list", "screen", "model", "child", "before", "after"):
                if key in ("child", "before", "after") and value in row_names:
                    continue
                resolve_map(path, value, "DeLa", key)
            elif element.tag == "widget" and key == "name" and "\\" in value:
                errors.append(f"{path}: name={value} names a map widget in an Xbox-look file")
            elif not patch and key in ("widget", "description", "root"):
                if "\\" in value:
                    errors.append(f"{path}: {key}={value!r} names a map widget in an Xbox-look file")
                else:
                    resolve(path, value, "DeLa", key)
            elif key in ("bitmap", "header_bitmap", "footer_bitmap", "font", "string_list"):
                group = {"font": "font", "string_list": "ustr"}.get(key, "bitm")
                resolve(path, value, group, key)
        for child in element:
            visit(path, child, patch, row_names)
            if patch and child.tag in ("row", "replace") and child.get("name"):
                row_names.add(child.get("name"))

    for path, root in documents:
        visit(path, root)
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("map", type=Path, nargs="?", help="an Xbox ui.map (version 5)")
    parser.add_argument("--output", type=Path, help="write JSON here instead of stdout")
    parser.add_argument("--check-menus", type=Path, help="check an Xbox menu XML folder")
    parser.add_argument("--assets", type=Path, help="PNG base folder (default: menu folder's parent)")
    args = parser.parse_args(argv)
    if args.map is None and args.check_menus is None:
        parser.error("provide a map or --check-menus")
    try:
        inventory = read_map(args.map) if args.map is not None else None
        if args.check_menus:
            errors = check_menus(args.check_menus, args.assets, inventory)
            if errors:
                print("\n".join(errors), file=sys.stderr)
                return 1
        if inventory is not None:
            output = json.dumps(inventory, indent=2, ensure_ascii=False) + "\n"
            if args.output:
                args.output.write_text(output, encoding="utf-8")
            else:
                print(output, end="")
    except (OSError, ValueError) as error:
        print(f"ui_map_tags: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
