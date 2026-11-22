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

The rules are the shader record contract's (port/runtime/shader_record.h), applied by the `shader-record` tool
(port/ios/tests/shader_record_tool.c): a recorded file is read with the app's own admission rules, pixel shader
keys are written canonical, the result must be closed (every pipeline's vertex and pixel shaders in the list)
and pass the tool-side lint, and the lists are changed only if every map passes: a bad hash, an out-of-range
field or a pipeline whose dependencies are in no list exits nonzero and leaves port/shader-lists as it was. A
file harvested from a device is a delta (it holds only what the device compiled outside the warm-up): close it
first with `shader-record close` (runbooks/benchmark-results.md, "Extract and harvest").
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LISTS = ROOT / "port/shader-lists"
ORDER = {"vs": 0, "ps": 1, "pipeline": 2}


def tool():
    """the built shader-record tool (tools/data_pack.py builds it from the same sources the app admits with)"""
    try:
        import data_pack
    except ImportError:
        from tools import data_pack
    return str(data_pack.shader_record_tool())


def read_lines(path):
    """the list's entries (no comments or blanks)"""
    if not path.is_file():
        return set()
    return {" ".join(line.split()) for line in path.read_text().splitlines()
            if line.strip() and not line.startswith("#") and line.split()[0] in ORDER}


def recorded_lists(folder):
    """the MAP.txt files in a run's --out folder or in a folder of them"""
    folder = Path(folder)
    for candidate in (folder / "runner/shader-lists", folder):
        paths = sorted(candidate.glob("*.txt")) if candidate.is_dir() else []
        if paths:
            return paths
    sys.exit(f"{folder}: no recorded lists (neither runner/shader-lists/MAP.txt nor MAP.txt files)")


def run_tool(arguments, lists):
    """runs the tool into a private folder; returns it only if the tool passed every map"""
    result = subprocess.run([tool(), *arguments], capture_output=True, text=True)
    if result.returncode:
        sys.exit(f"shader_lists: {lists} left unchanged\n" + (result.stderr.strip() or result.stdout.strip()))
    return result


def install(staged, lists):
    """copies the staged maps over the lists; returns {map: (lines before, lines after)}"""
    lists.mkdir(parents=True, exist_ok=True)
    sizes = {}
    for path in sorted(staged.glob("*.txt")):
        before = read_lines(lists / path.name)
        sizes[path.stem] = (len(before), len(read_lines(path)))
        if not (lists / path.name).exists() or (lists / path.name).read_bytes() != path.read_bytes():
            shutil.copyfile(path, lists / path.name)
    return sizes


def merge(folders, lists=LISTS):
    """adds the folders' lines to the lists; returns how many each map gained"""
    folders = [recorded_lists(folder)[0].parent for folder in folders]
    gained = {}
    with tempfile.TemporaryDirectory(prefix="visr-shader-lists-") as temporary:
        staged = Path(temporary) / "out"
        run_tool(["merge", "--out", str(staged), str(lists), *map(str, folders)], lists)
        for name, (before, after) in install(staged, lists).items():
            gained[name] = after - before
            if name in {path.stem for folder in folders for path in folder.glob("*.txt")}:
                print(f"{name}: {after - before} lines added ({after} in all)")
    return gained


def canonicalize(lists=LISTS):
    """rewrites every list with canonical pixel shader keys; returns {map: (lines before, lines after)}"""
    with tempfile.TemporaryDirectory(prefix="visr-shader-lists-") as temporary:
        staged = Path(temporary) / "out"
        run_tool(["canonical", "--out", str(staged), str(lists)], lists)
        sizes = install(staged, lists)
    for name, (before, after) in sizes.items():
        print(f"{name}: {before} -> {after} lines")
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
