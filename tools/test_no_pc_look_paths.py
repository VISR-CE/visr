"""No tag path or picture folder of the retired PC look.

The port's own menu tags live under `pc\\`: the generated Xbox-look screens under `pc\\xbox\\` and the rows a patch
adds under `pc\\patch\\`. Every other `pc\\...` tag path named the PC look's screens, string lists and pictures, which
are gone, so a reference to one finds nothing (or needed an alias table to find its replacement). This test fails on
any such path in a tracked file. The changelog keeps its history, other projects' code is not ours, and this file holds the scan's own examples.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
# The port's live tag namespaces under pc\.
LIVE = ('xbox', 'patch')
# A tag path: `pc` then backslashes (one, or escaped) or a slash, then a name; not a folder such as cache/pc/v1.
PATH = re.compile(r'(?<![A-Za-z0-9_/])pc(?:\\+|/)(?!(?:' + '|'.join(LIVE) + r')(?:\\|/))[A-Za-z_]')
# The PC look's picture folder, `menus/ce/` (now `menus/pictures/`), as a path a generator writes or a file names.
FOLDER = re.compile(r'''(?:menus/ce/|["'f]ce/[A-Za-z_{])''')
SKIP = ('CHANGELOG.md', 'tools/test_no_pc_look_paths.py')
SKIP_PREFIXES = ('port/third_party/', 'LICENSES/')


def tracked():
    out = subprocess.run(['git', 'ls-files', '-z'], cwd=ROOT, capture_output=True, check=True).stdout.decode()
    return [path for path in out.split('\0') if path and path not in SKIP and not path.startswith(SKIP_PREFIXES)]


def retired_paths(text):
    return [number for number, line in enumerate(text.splitlines(), 1) if PATH.search(line) or FOLDER.search(line)]


def test_no_tracked_file_names_a_pc_look_tag():
    found = []
    for path in tracked():
        try:
            text = (ROOT / path).read_text(encoding='utf-8')
        except (UnicodeDecodeError, OSError):
            continue
        found += [f'{path}:{line}' for line in retired_paths(text)]
    assert not found, f'tag paths of the retired PC look: {found}'


def test_the_scan_tells_retired_paths_from_live_ones():
    assert retired_paths('string_get("pc\\\\main_menu\\\\map_list", 0, text);')
    assert retired_paths("name = 'pc/main_menu/main_menu'")
    assert retired_paths('#define NAME "pc\\error\\confirm"')
    assert not retired_paths('#define LOBBY_NAME "pc\\\\xbox\\\\main_menu\\\\lobby_screen"')
    assert not retired_paths('tag_loaded(TAG, "pc\\\\patch\\\\multiplayer_type_internet")')
    assert not retired_paths('#define PC_MENU_TAG_PREFIX "pc\\\\"')
    assert not retired_paths('cache="cache/pc/v1/name__0.png"')
    assert retired_paths('png = f"ce/{relative}__{index}.png"')
    assert retired_paths("copytree(ROOT / 'port/assets/menus/ce/x')")
    assert retired_paths('<frame png="ce/shell/bitmaps/a__0.png"/>')
    assert not retired_paths('png = f"pictures/{relative}__{index}.png"')
    assert not retired_paths('strcpy(list[1].family, "ce");')
