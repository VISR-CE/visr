#!/usr/bin/env python3
"""Build and run the texture repair filter's property tests (synthetic gradients, no map data)."""
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/ios/probe/texture-repair"


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    cc = ["xcrun", "clang"] if os.uname().sysname == "Darwin" else ["cc"]
    flags = ["-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=undefined", f"--include-directory={ROOT / 'port/runtime'}"]
    probe = BUILD / "probe"
    subprocess.run([*cc, *flags, "port/ios/tests/texture_repair_probe.c", "port/runtime/texture_repair.c",
                    "-lm", "--output", str(probe)], cwd=ROOT, check=True)
    subprocess.run([str(probe)], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
