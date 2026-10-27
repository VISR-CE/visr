#!/usr/bin/env python3
"""Compare two recordings/seed-trace.log files (debug.seed_trace; port/linux/game/seed_trace.c) and name the first tick
and line where they part.

    seed_trace_diff.py A.log B.log [--context N]

Lines are grouped by tick (B ... E). The first tick whose lines differ is reported with its first differing line and
the lines before it, in order: the action (A), the draws (G global, L local) and the world checksum at the tick's end (E).
Exit status 0 when the traces agree over the shorter run, 1 when they part.
"""

import argparse
import sys


def ticks(path, ignore_local=False):
    """the file's lines grouped by tick: {tick: [lines]}, plus the lines before the first tick"""
    groups, head, current = {}, [], None
    for line in open(path):
        line = line.rstrip("\n")
        if ignore_local:
            if line.startswith("L ") or line.startswith("S clock_seed"):
                continue
            if line.startswith("B "):
                line = " ".join(line.split()[:3])
            elif line.startswith("E "):
                line = " ".join(line.split()[:-1])
        if line.startswith("B "):
            current = int(line.split()[1])
            groups[current] = []
        (head if current is None else groups[current]).append(line)
    return head, groups


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("a")
    parser.add_argument("b")
    parser.add_argument("--context", type=int, default=6)
    parser.add_argument("--ignore-local", action="store_true",
                        help="leave out the local sequence (L lines and the local seed in B and E lines): the clock seeds it, "
                             "so two runs never agree on it, and the question is whether the simulation parts")
    args = parser.parse_args()
    head_a, a = ticks(args.a, args.ignore_local)
    head_b, b = ticks(args.b, args.ignore_local)
    print(f"{args.a}: {len(a)} ticks; {args.b}: {len(b)} ticks")
    if head_a != head_b:
        print("before the first tick the traces differ:")
        for x, y in zip(head_a, head_b):
            if x != y:
                print(f"  A: {x}\n  B: {y}")
    # (the two runs may begin at different tick numbers: compare tick by tick in order, and report both numbers)
    for index, (ta, tb) in enumerate(zip(sorted(a), sorted(b))):
        la, lb = a[ta], b[tb]
        # the seeds a tick begins with and the call sites are the same in a good run; the tick numbers are relative
        norm = lambda lines, t: [" ".join(x.split()[:1] + [str(int(x.split()[1]) - t)] + x.split()[2:]) if x[0] in "AGLE" else x for x in lines]
        na, nb = norm(la, ta), norm(lb, tb)
        if na != nb:
            for line_index, (x, y) in enumerate(zip(na, nb)):
                if x != y:
                    break
            else:
                line_index = min(len(na), len(nb))
            print(f"first difference at the {index}th tick (A tick {ta}, B tick {tb}), line {line_index}:")
            for line in la[max(0, line_index - args.context):line_index]:
                print("   ", line)
            print("  A:", la[line_index] if line_index < len(la) else "(ends)")
            print("  B:", lb[line_index] if line_index < len(lb) else "(ends)")
            return 1
    print("the traces agree over the shorter run")
    return 0


if __name__ == "__main__":
    sys.exit(main())
