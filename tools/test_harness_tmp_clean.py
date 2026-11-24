"""Native and Python test programs leave no temporary folder behind.

A test that made a folder under a hard-coded /tmp path and removed it only on its success path left one folder per
failed or aborted run; tools/harness/tests/map_share_send.c alone left more than four thousand (20 GB) on a job host.
Native tests make their folders with test_mkdtemp (port/ios/tests/test_tmpdir.h: under $TMPDIR, removed at exit,
on a failed CHECK and on an abort); Python tests remove theirs at exit. This test refuses a source that does
otherwise, and runs the native helper on every way a program can end.
"""

import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "port/ios/tests/test_tmpdir.h"
NATIVE_TESTS = sorted([*(ROOT / "port/ios/tests").glob("*.[cm]"), *(ROOT / "port/ios/tests").glob("*.cpp"),
                       *(ROOT / "tools/harness/tests").glob("*.c"), *(ROOT / "tools").glob("*.c")])
PYTHON_TESTS = sorted([*(ROOT / "tools").glob("*.py"), *(ROOT / "tools/harness/tests").glob("*.py")])


def native_leaks(text):
    """What in a native test's source makes a folder (or a file) outside the helper."""
    return (re.search(r"(?<![\w])mkdtemp\s*\(", text) or re.search(r"(?<![\w])mkstemp\s*\(", text)
            or re.search(r'"/tmp/[^"]*XXXXXX', text))


@pytest.mark.parametrize("path", NATIVE_TESTS, ids=lambda path: str(path.relative_to(ROOT)))
def test_native_test_makes_its_folders_with_the_helper(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    assert not native_leaks(text), f"{path.name} makes a temporary folder or file itself: use test_mkdtemp (test_tmpdir.h)"


def test_negative_control_the_detector_sees_the_leaking_forms():
    assert native_leaks('char d[] = "/tmp/probe-XXXXXX"; mkdtemp(d);')
    assert native_leaks("int fd = mkstemp(name);")
    assert not native_leaks('test_mkdtemp(d, sizeof(d), "probe");')


@pytest.mark.parametrize("path", PYTHON_TESTS, ids=lambda path: str(path.relative_to(ROOT)))
def test_python_test_removes_its_folders(path):
    if path == Path(__file__).resolve():
        return
    text = path.read_text(encoding="utf-8", errors="replace")
    if "mkdtemp(" in text:
        assert "rmtree" in text, f"{path.name} calls tempfile.mkdtemp and never removes the folder"


PROGRAM = r"""
#include <assert.h>
#include <sys/wait.h>
#include "test_tmpdir.h"
int main(int count, char **arguments)
{
	char folder[PATH_MAX], file[PATH_MAX + 16];
	FILE *stream;

	if (!test_mkdtemp(folder, sizeof(folder), "tmp-clean-probe"))
		return 3;
	snprintf(file, sizeof(file), "%s/inner", folder);
	mkdir(file, 0755);
	snprintf(file, sizeof(file), "%s/inner/file", folder);
	stream = fopen(file, "w");
	fputs("x", stream);
	fclose(stream);
	if (count > 1 && !strcmp(arguments[1], "exit"))
		exit(1);
	if (count > 1 && !strcmp(arguments[1], "abort"))
		assert(count < 0);
	if (count > 1 && !strcmp(arguments[1], "segv"))
		raise(SIGSEGV);
	if (count > 1 && !strcmp(arguments[1], "child"))
	{
		pid_t child = fork();
		if (child == 0)
			exit(0);
		waitpid(child, NULL, 0);
		snprintf(file, sizeof(file), "%s/inner/file", folder);
		return access(file, F_OK) == 0 ? 0 : 4;   /* the child's exit left the parent's folder alone */
	}
	return 0;
}
"""


SHORT = r"""
#include "test_tmpdir.h"
int main(void)
{
	char folder[64];
	return test_mkdtemp(folder, sizeof(folder), "short-buffer") == NULL ? 0 : 1;
}
"""


@pytest.fixture(scope="module")
def program(tmp_path_factory):
    work = tmp_path_factory.mktemp("tmp-clean-build")
    source = work / "probe.c"
    source.write_text(PROGRAM)
    binary = work / "probe"
    result = subprocess.run([shutil.which("clang") or "cc", "-O0", "-Wall", "-Werror", "-I", str(HEADER.parent),
                             str(source), "-o", str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    return binary


@pytest.mark.parametrize("ending", ["return", "exit", "abort", "segv", "child"])
def test_helper_removes_its_folder_however_the_program_ends(program, tmp_path, ending):
    environment = {"TMPDIR": str(tmp_path), "PATH": "/usr/bin:/bin"}
    result = subprocess.run([str(program), ending], env=environment, capture_output=True)
    assert result.returncode == {"return": 0, "exit": 1, "abort": -6, "segv": -11, "child": 0}[ending], result.stderr
    assert list(tmp_path.iterdir()) == []


def test_keep_test_tmp_leaves_the_folder_for_debugging(program, tmp_path):
    environment = {"TMPDIR": str(tmp_path), "PATH": "/usr/bin:/bin", "KEEP_TEST_TMP": "1"}
    subprocess.run([str(program), "return"], env=environment, check=True)
    assert len(list(tmp_path.iterdir())) == 1


def test_a_real_probe_leaves_nothing_in_its_temp_root(tmp_path):
    """The class check on a real probe, built the way ios_test.py builds it: its folders are gone after it ends."""
    binary = tmp_path / "config-probe"
    root = tmp_path / "root"
    root.mkdir()
    try:
        flags = subprocess.check_output(["pkg-config", "--cflags", "--libs", "sdl3"], text=True).split()
    except (OSError, subprocess.CalledProcessError):
        pytest.skip("sdl3 is not installed")
    build = subprocess.run([shutil.which("clang") or "cc", "-O2", "-Wl,-dead_strip", "-DHALO_ILP32=1", "-I" + str(ROOT / "port/linux/src"),
                            "-I" + str(ROOT / "port/third_party/tomlc17"), str(ROOT / "port/ios/tests/config_probe.c"),
                            *flags, "-o", str(binary)], capture_output=True, text=True)
    assert build.returncode == 0, build.stderr[-500:]
    environment = {"TMPDIR": str(root), "PATH": "/usr/bin:/bin"}
    run = subprocess.run([str(binary)], env=environment, capture_output=True, cwd=ROOT)
    assert run.returncode == 0, run.stderr
    assert list(root.iterdir()) == []
    subprocess.run([str(binary)], env={**environment, "KEEP_TEST_TMP": "1"}, capture_output=True, cwd=ROOT, check=True)
    assert list(root.iterdir()) != []   # negative control: the folder the probe made is visible when kept


@pytest.mark.parametrize("language", ["c", "c++", "objective-c"])
def test_helper_compiles_as_every_language_the_tests_use(tmp_path, language):
    source = tmp_path / "probe.c"
    source.write_text(PROGRAM)
    result = subprocess.run([shutil.which("clang") or "cc", "-x", language, "-O0", "-Wall", "-I", str(HEADER.parent),
                             "-fsyntax-only", str(source)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def test_a_template_that_does_not_fit_is_refused_not_cut(tmp_path):
    source = tmp_path / "short.c"
    source.write_text(SHORT)
    binary = tmp_path / "short"
    result = subprocess.run([shutil.which("clang") or "cc", "-I", str(HEADER.parent), str(source), "-o", str(binary)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    long_root = tmp_path / ("x" * 60)
    long_root.mkdir()
    assert subprocess.run([str(binary)], env={"TMPDIR": str(long_root)}).returncode == 0
    assert list(long_root.iterdir()) == []
