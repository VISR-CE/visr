#!/usr/bin/env python3
"""Regenerate linux_headers.txt and linux_libm.txt from pinned Ubuntu 24.04 packages.

The strict compiler wrappers in this folder use the two lists to imitate the
Tooling job's Linux runner on a Mac: every system header a probe includes must
exist on Ubuntu 24.04, and every libm function a probe calls must be linked
with -lm. The packages are the noble release pocket, which the archive keeps.

Needs curl, ar, zstd, tar and an ELF-capable nm (Xcode's nm is llvm-nm).
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
MIRROR = "http://archive.ubuntu.com/ubuntu/"
PACKAGES = {
    "libc6-dev": ("pool/main/g/glibc/libc6-dev_2.39-0ubuntu8_amd64.deb",
                  "1f03b79223d9cf1a6ed3a648f6942619dcb97b2884dba8fd12e262b6771e247a"),
    "libc6": ("pool/main/g/glibc/libc6_2.39-0ubuntu8_amd64.deb",
              "af36c7ac770770fe3d3c10e85d6bc538e76e57570ba7db7d397fb9f654783ef3"),
    "linux-libc-dev": ("pool/main/l/linux/linux-libc-dev_6.8.0-31.31_amd64.deb",
                       "21c631ba4da3d38d92a133e28a8a14278e1e00174ed135a20661d1e532e05ca5"),
    "libgcc-13-dev": ("pool/main/g/gcc-13/libgcc-13-dev_13.2.0-23ubuntu4_amd64.deb",
                      "d36c9acb969dd37fdc0496120715d8bba67a5cc43a0c2569717314e6ad80020c"),
    # Preinstalled on GitHub's ubuntu-24.04 runner image, which the Tooling job uses.
    "zlib1g-dev": ("pool/main/z/zlib/zlib1g-dev_1.3.dfsg-3.1ubuntu2_amd64.deb",
                   "7360405fbf49e35dca2f7a4ebeac7d4fe5fe3b2db99adac70e11e104f53b2642"),
}
DEVELOPMENT = ("libc6-dev", "linux-libc-dev", "libgcc-13-dev", "zlib1g-dev")
# Header roots, as include names are seen from: glibc and the kernel headers,
# their multiarch copies, and gcc's own headers.
INCLUDE_ROOTS = ("usr/include/x86_64-linux-gnu", "usr/include",
                 "usr/lib/gcc/x86_64-linux-gnu/13/include")


def unpack(name: str, work: Path) -> Path:
    path, digest = PACKAGES[name]
    deb = work / Path(path).name
    subprocess.run(["curl", "--silent", "--show-error", "--fail", "--max-time", "300",
                    "--output", str(deb), MIRROR + path], check=True)
    actual = hashlib.sha256(deb.read_bytes()).hexdigest()
    if actual != digest:
        raise SystemExit(f"{deb.name}: SHA-256 {actual}, expected {digest}")
    members = work / f"{name}.ar"
    members.mkdir()
    subprocess.run(["ar", "x", str(deb)], cwd=members, check=True)
    data = next(members.glob("data.tar.*"))
    tar = members / "data.tar"
    if data.suffix == ".zst":
        subprocess.run(["zstd", "--decompress", "--quiet", "--force", str(data), "--output-dir-flat", str(members)],
                       check=True)
    else:
        tar = data
    root = work / name
    root.mkdir()
    subprocess.run(["tar", "--extract", "--file", str(tar), "--directory", str(root)], check=True)
    return root


def headers(roots: list[Path]) -> set[str]:
    names = set()
    for root in roots:
        for path in root.rglob("*.h"):
            relative = path.relative_to(root).as_posix()
            for prefix in INCLUDE_ROOTS:
                if relative.startswith(prefix + "/"):
                    names.add(relative[len(prefix) + 1:])
                    break
    return names


def exports(library: Path) -> set[str]:
    output = subprocess.run(["nm", "--dynamic", "--defined-only", str(library)],
                            check=True, capture_output=True, text=True).stdout
    names = set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[1] in "TWiV":
            names.add(fields[2].split("@")[0])
    return names


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="fail if the committed lists differ")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        roots = {name: unpack(name, work) for name in PACKAGES}
        header_names = headers([roots[name] for name in DEVELOPMENT])
        runtime = roots["libc6"] / "usr/lib/x86_64-linux-gnu"
        # Functions libm exports that libc does not: these link on macOS (libSystem) but need -lm on Linux.
        libm_only = exports(runtime / "libm.so.6") - exports(runtime / "libc.so.6")
    outputs = {
        HERE / "linux_headers.txt": header_names,
        HERE / "linux_libm.txt": libm_only,
    }
    stale = []
    for path, names in outputs.items():
        text = "".join(f"{name}\n" for name in sorted(names))
        if args.check:
            if not path.exists() or path.read_text() != text:
                stale.append(path.name)
        else:
            path.write_text(text)
            print(f"{path.name}: {len(names)} names")
    if stale:
        raise SystemExit("stale: " + ", ".join(stale) + "; run tools/probekit/strict/refresh_lists.py")


if __name__ == "__main__":
    main()
