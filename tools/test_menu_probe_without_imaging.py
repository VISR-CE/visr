"""The menu-builder probe (ios_test's `test_menu_builder_loads.py`) imports without numpy or Pillow.

Bug class: a name lookup (`engine_names`) lived in `ce_menus`, whose imports need numpy and Pillow (the SVG renderer), so a
host without them (the MacBook, a bare Homebrew python) failed ios_test's menu-builder step on an unrelated import. The
probe needs only the standard library and `menu_probe_build`.
"""
from pathlib import Path
import subprocess
import sys

TOOLS = Path(__file__).resolve().parent


def test_menu_builder_probe_imports_without_numpy_or_pillow():
    code = ("import sys; sys.modules['numpy'] = None; sys.modules['PIL'] = None; sys.path.insert(0, %r); "
            "import test_menu_builder_loads, menu_engine_names; assert callable(menu_engine_names.engine_names)"
            ) % str(TOOLS)
    result = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
