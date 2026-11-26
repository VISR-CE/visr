#!/usr/bin/env python3
"""Check the texture repair on every texture of every map under a data root (runner hosts only: the stock maps are not
in the repository). Builds port/ios/tests/texture_repair_sweep.m beside the filter and runs it; exit status 1 means a
texel was repaired outside the stored rounding or a texel outside a flagged area changed.

    run-job.sh --kind python LABEL WORKTREE python3 tools/texture_repair_sweep.py [DATA_ROOT]

DATA_ROOT defaults to $HALO_REMOTE_ROOT/data/Documents (maps/ and custom_maps/ below it).
"""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("data_root", nargs="?", type=Path)
    parser.add_argument("--table", type=Path, help="write the per-texture rows here (default: standard output)")
    args = parser.parse_args()
    data = args.data_root or Path(os.environ.get("HALO_REMOTE_ROOT", "")) / "data/Documents"
    if not (data / "maps").is_dir():
        sys.exit(f"no maps folder under {data}")
    includes = [f"--include-directory={ROOT / p}" for p in
                ("port/linux/src", "port/ios/host", "port/runtime", "port/third_party/astcenc", "port/runtime/include")]
    with tempfile.TemporaryDirectory(prefix="texture-repair-sweep-") as temp:
        build = Path(temp)
        objects = []
        sources = ["port/runtime/texture_repair.c", "port/linux/src/astc_rewrite.c", "port/runtime/astc_store.c",
                   "port/ios/host/host_map_identity.m", "port/ios/tests/texture_repair_sweep.m",
                   "port/linux/src/astc_encode.cpp", "port/ios/host/host_astc_texture.c", "port/ios/host/metal_gate.m"]
        sources += [str(p.relative_to(ROOT)) for p in sorted((ROOT / "port/third_party/astcenc").glob("*.cpp"))]
        for index, source in enumerate(sources):
            cxx = source.endswith(".cpp")
            command = ["xcrun", "clang++" if cxx else "clang", "-O2", "-g", "-DASTCENC_BLOCK_MAX_TEXELS=16", *includes]
            if cxx:
                command += ["-std=c++14"]
            if source.endswith(".m"):
                command += ["-fobjc-arc", "-fblocks", "-DMETAL_GATE_NO_METALFX"]
            obj = build / f"{index}.o"
            subprocess.run([*command, "-c", str(ROOT / source), "--output", str(obj)], check=True)
            objects.append(str(obj))
        executable = build / "sweep"
        subprocess.run(["xcrun", "clang++", *objects, "-framework", "Foundation", "-framework", "Metal", "-framework",
                        "AppKit", "-lz", "--output", str(executable)], check=True)
        output = args.table.open("w") if args.table else None
        try:
            result = subprocess.run([str(executable), str(data)], stdout=output)
        finally:
            if output:
                output.close()
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
