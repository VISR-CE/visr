"""ce_menus.engine_names (a name lookup in a C source) must not need numpy.

Bug class: a module-level import of a heavy dependency (hud_assets needs numpy) in a module whose small helpers other
scripts import, so a host without that dependency (the MacBook's python) fails ios_test's menu-builder step on an
unrelated import. Only drawing (render_svg, bleed) needs hud_assets, and it is imported on first use.
"""
from pathlib import Path
import subprocess
import sys

TOOLS = Path(__file__).resolve().parent


def test_engine_names_imports_without_numpy():
    # PIL is stubbed too: the strict CI environment has neither, and only numpy is under test
    code = ("import sys, types; sys.modules['numpy'] = None; sys.modules['PIL'] = types.ModuleType('PIL'); "
            "sys.modules['PIL'].Image = sys.modules['PIL.Image'] = types.ModuleType('PIL.Image'); "
            "sys.path.insert(0, %r); "
            "from ce_menus import engine_names; assert callable(engine_names); "
            "assert 'hud_assets' not in sys.modules") % str(TOOLS)
    result = subprocess.run([sys.executable, "-I", "-c", code], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
