"""The background redraw must leave unsupported sources unchanged."""
import pathlib
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def test_background_redraw_guard():
    compiler = shutil.which("clang") or shutil.which("cc")
    assert compiler
    with tempfile.TemporaryDirectory() as directory:
        executable = pathlib.Path(directory) / "background-probe"
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I" + str(ROOT / "port/ios/host"),
                        str(ROOT / "port/ios/tests/menu_background_redraw_probe.c"),
                        str(ROOT / "port/ios/host/menu_background_redraw.c"),
                        "-lm", "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
