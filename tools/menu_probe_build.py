"""The menu probes' build: port/linux/game/menu_tags.c and the menu file reader, compiled beside the stand-in headers of
port/ios/tests/menu_patch (the sources are copied there, because their own directories' headers would win over the
stand-ins). tools/test_menu_patches.py and tools/test_menu_builder_loads.py build their probes with it, through the
shared probe build (tools/probekit/cc.py)."""
from pathlib import Path
import shutil

from probekit import cc

ROOT = Path(__file__).resolve().parents[1]
EXPAT = ROOT / "port/third_party/expat"
SHARED_SOURCES = ("port/linux/game/menu_tags.c", "port/linux/src/menu_files.c", "port/linux/src/menu_files.h",
                  "port/linux/src/menu_platform.h", "port/linux/include/halo_menus.h",
                  "port/linux/include/menu_equivalence.h", "port/linux/include/menu_geometry.h",
                  "port/linux/include/halo_menu_meta.h")


def build_probe(directory: Path, probe: str, extra_sources: tuple = ()) -> Path:
    """`probe` is the path (from the repository root) of the probe's C file; extra_sources are C files already in
    directory (generated beside it). Returns the binary, named for the probe."""
    shutil.copytree(ROOT / "port/ios/tests/menu_patch", directory, dirs_exist_ok=True)
    for source in (*SHARED_SOURCES, probe):
        shutil.copy2(ROOT / source, directory)
    name = Path(probe).stem
    binary = directory / name
    sources = [directory / f"{name}.c", directory / "menu_files.c", *extra_sources,
               *(EXPAT / part for part in ("xmlparse.c", "xmlrole.c", "xmltok.c"))]
    return cc.build(sources, output=binary, apple=True, flags=[
        "-std=gnu11", "-g", "-Wall", "-Werror", "-Wno-multichar", "-Wno-unused-function", "-Wno-unused-variable",
        "-Wno-unused-but-set-variable", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
        "-DHALO_ILP32=1", "-DHALO_IOS=1", "-DMENU_TAGS_PROBE=1", f"--include-directory={directory}",
        f"--include-directory={EXPAT}", f"--include-directory={ROOT / 'port/linux/game'}"])
