#!/usr/bin/env python3
"""Check that changing the shadow maps' size while the game runs draws what the same size does from launch.

Two runs of b30's landing on the Mac runner (tools/mac_run.py, a built Mac app: tools/ios_build.py --mac), each
with a fixed timestep so their frames line up: one with display.shadow_map_size 256 from launch, one starting at 128
and changed to 256 at frame 1230 (debug.knob_at, as debug.knob_cycle's chord would). The Pelican's and the marines'
shadows are on the beach then. The scene-only screenshots after the change must match the launch run's, and the one
before it must not (else the frames show no object shadows to compare).

    tools/shadow_resize_check.py OUT [--runner native|ipad] [--keep]

Exit 0 when they match. Before the fix, every object shadow was gone after the change (80,847 pixels differed at
frame 1245 against none after it).
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SWITCH_FRAME = 1230
BEFORE = [1215]                 # the sizes differ: shadows are in the picture
AFTER = [1245, 1260, 1275]      # the change has taken: the pictures match
EXIT_AFTER = 44                 # game seconds at the fixed timestep's 1/30 s a frame: past frame 1275
TOLERANCE = 64                  # pixels a matching pair may differ by
DIFFERENT = 5000                # pixels a pair of different sizes must differ by, at least


def run(out, runner, sets):
    command = [sys.executable, str(ROOT / "tools/mac_run.py"), "run", "--runner", runner, "--out", str(out),
               "--exit-after", str(EXIT_AFTER), "--screenshot-every", "15",
               "--set", "debug.fixed_timestep=true", "--set", "display.render_height=720",
               "--init", "cheat_deathless_player true", "--init", "map_name b30"]
    for assignment in sets:
        command += ["--set", assignment]
    result = subprocess.run(command, cwd=ROOT)
    if result.returncode:
        sys.exit(f"shadow_resize_check: the run into {out} failed ({result.returncode})")


def scene(out, frame):
    path = out / "runner" / "shots" / f"frame{frame:05d}-scene.png"
    if not path.exists():
        sys.exit(f"shadow_resize_check: {path} is missing")
    return path.read_bytes()


def differing(a, b):
    sys.path.insert(0, str(ROOT / "tools"))
    from mac_run import bmp_difference  # (mac_run's PNG reader: no imaging module needed)

    return bmp_difference(a, b)[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("out", type=Path)
    parser.add_argument("--runner", choices=("native", "ipad"), default="native")
    parser.add_argument("--keep", action="store_true", help="keep the runs' screenshots")
    args = parser.parse_args()
    launch, live = args.out / "launch-256", args.out / "live-128-to-256"
    for folder in (launch, live):
        shutil.rmtree(folder, ignore_errors=True)
    run(launch, args.runner, ["display.shadow_map_size=256"])
    run(live, args.runner, ["display.shadow_map_size=128",
                            f'debug.knob_at="{SWITCH_FRAME}:display.shadow_map_size=256"'])
    log = (live / "stderr.log").read_text(errors="replace")
    ok = True
    if not re.search(r"knob: display\.shadow_map_size=256 \(debug\.knob_at, frame %d\)" % SWITCH_FRAME, log):
        print("FAIL: the live run never logged the change at frame %d" % SWITCH_FRAME)
        ok = False
    for frame in BEFORE:
        count = differing(scene(launch, frame), scene(live, frame))
        print(f"frame {frame} (before the change, 256 against 128): {count} pixels differ")
        if count < DIFFERENT:
            print(f"FAIL: under {DIFFERENT} pixels, so the frame shows too little shadow to check")
            ok = False
    for frame in AFTER:
        count = differing(scene(launch, frame), scene(live, frame))
        print(f"frame {frame} (after the change, both 256): {count} pixels differ")
        if count > TOLERANCE:
            print(f"FAIL: over {TOLERANCE} pixels")
            ok = False
    if not args.keep:
        for folder in (launch, live):
            shutil.rmtree(folder / "runner" / "shots", ignore_errors=True)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
