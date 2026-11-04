#!/usr/bin/env python3
"""Merge recorded shader lists into the repository's (port/shader-lists).

The app appends the shaders and pipelines its draws made that a map's list did not have to MAP.txt in its
`debug.shader_list_record` folder (`shader-lists-missed` in its data folder; d3d8_device.c, "shader lists"), and
a run with `mac_run.py run --record-shader-lists` leaves them in runner/shader-lists in its --out folder. This adds
their lines to port/shader-lists/MAP.txt, keeping the lines already there, without duplicates, vertex shaders
first, then pixel shaders, then pipelines, each sorted. Each FOLDER is a run's --out folder or a folder of
MAP.txt files (a `shader-lists-missed` folder pulled from a device or a runner).

    python3 tools/shader_lists.py merge FOLDER [FOLDER...]
    python3 tools/shader_lists.py canonicalize
    python3 tools/shader_lists.py stats

Pixel shader keys are listed canonical: the combiner registers at or past the key's combiner count are zero
(nv2a_pixel_shader_key_canonicalize in xgpu.h; the translation reads only the stages below the count), and a ps line's
hash is the key's FNV-1a hash. Merging canonicalizes what it reads, and a pipeline line names its pixel shader by
the canonical hash, so lists recorded by older builds merge with newer ones.
"""
import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LISTS = ROOT / "port/shader-lists"
ORDER = {"vs": 0, "ps": 1, "pipeline": 2}
HEADER = ("# {map}: the shaders and pipelines this map's draws make, compiled while it loads\n"
          "# (d3d8_device.c, halo_shader_list_warm); merged by tools/shader_lists.py from recorded runs\n")


# a pipeline line from before it carried the separate alpha factors: `pipeline VS PS BLEND SOURCE DESTINATION
# OPERATION MASK EXACT DEPTH KINDS`, to which the app now adds ALPHA_SOURCE ALPHA_DESTINATION
OLD_PIPELINE_FIELDS = 11


def normalized(line):
    """a line in its current form: an old pipeline line blends alpha as color, as the app reads it"""
    fields = line.split()
    if fields[0] == "pipeline" and len(fields) == OLD_PIPELINE_FIELDS:
        fields += [fields[4], fields[5]]
    return " ".join(fields)


# the keys of the 32-bit guest (struct nv2a_pixel_shader_key, xgpu.h): D3DRS_PS_MAX combiner words come first
COMBINER_WORDS = 57
COMBINER_COUNT = 53
STAGE_REGISTERS = (0, 26, 34, 45)  # alpha inputs, alpha outputs, rgb inputs, rgb outputs (D3DRS_PS...0)


def fnv1a64(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


def canonical_key(key):
    """key (bytes) with the combiner registers of the stages past its combiner count zeroed; a key too short to
    hold the combiner state is returned as it is"""
    if len(key) < 4 * COMBINER_WORDS:
        return key
    words = list(struct.unpack_from(f"<{COMBINER_WORDS}I", key))
    for stage in range(min(words[COMBINER_COUNT] & 0xff, 8), 8):
        for base in STAGE_REGISTERS:
            words[base + stage] = 0
    return struct.pack(f"<{COMBINER_WORDS}I", *words) + key[4 * COMBINER_WORDS:]


def canonical(lines):
    """the lines with every ps line's key canonical (and its hash the new key's), pipelines naming those hashes,
    the duplicates this makes dropped"""
    renamed = {}
    result = set()
    for line in lines:
        fields = line.split()
        if fields[0] == "ps" and len(fields) == 3 and len(fields[2]) % 2 == 0:
            key = canonical_key(bytes.fromhex(fields[2]))
            if len(key) >= 4 * COMBINER_WORDS:
                renamed[fields[1]] = f"{fnv1a64(key):016x}"
                line = f"ps {renamed[fields[1]]} {key.hex()}"
        result.add(line)
    out = set()
    for line in result:
        fields = line.split()
        if fields[0] == "pipeline" and len(fields) > 2 and fields[2] in renamed:
            fields[2] = renamed[fields[2]]
            line = " ".join(fields)
        out.add(line)
    return out


def read_lines(path):
    """the list's entries (no comments or blanks), each in its current form"""
    if not path.is_file():
        return set()
    return {normalized(line) for line in path.read_text().splitlines()
            if line.strip() and not line.startswith("#") and line.split()[0] in ORDER}


def write(lists, map_name, lines):
    ordered = sorted(lines, key=lambda line: (ORDER[line.split()[0]], line))
    (lists / f"{map_name}.txt").write_text(HEADER.format(map=map_name) + "".join(f"{line}\n" for line in ordered))


def recorded_lists(folder):
    """the MAP.txt files in a run's --out folder or in a folder of them"""
    folder = Path(folder)
    for candidate in (folder / "runner/shader-lists", folder):
        paths = sorted(candidate.glob("*.txt")) if candidate.is_dir() else []
        if paths:
            return paths
    sys.exit(f"{folder}: no recorded lists (neither runner/shader-lists/MAP.txt nor MAP.txt files)")


def merge(folders, lists=LISTS):
    """adds the folders' lines to the lists; returns how many each map gained"""
    lists.mkdir(parents=True, exist_ok=True)
    gained = {}
    for folder in folders:
        for path in recorded_lists(folder):
            existing = canonical(read_lines(lists / path.name))
            added = canonical(read_lines(path)) - existing
            write(lists, path.stem, existing | added)
            gained[path.stem] = gained.get(path.stem, 0) + len(added)
            print(f"{path.stem}: {len(added)} lines added from {folder} ({len(existing | added)} in all)")
    return gained


def canonicalize(lists=LISTS):
    """rewrites every list with canonical pixel shader keys; returns {map: (lines before, lines after)}"""
    sizes = {}
    for path in sorted(lists.glob("*.txt")):
        before = read_lines(path)
        after = canonical(before)
        write(lists, path.stem, after)
        sizes[path.stem] = (len(before), len(after))
        print(f"{path.stem}: {len(before)} -> {len(after)} lines")
    return sizes


def stats():
    for path in sorted(LISTS.glob("*.txt")):
        lines = read_lines(path)
        counts = {kind: sum(1 for line in lines if line.startswith(kind + " ")) for kind in ORDER}
        print(f"{path.stem}: {counts['vs']} vertex shaders, {counts['ps']} pixel shaders, "
              f"{counts['pipeline']} pipelines, {path.stat().st_size / 1024:.0f} KB")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("merge").add_argument("folders", nargs="+", type=Path)
    commands.add_parser("canonicalize")
    commands.add_parser("stats")
    args = parser.parse_args()
    if args.command == "merge":
        merge(args.folders)
    elif args.command == "canonicalize":
        canonicalize()
    else:
        stats()


if __name__ == "__main__":
    main()
