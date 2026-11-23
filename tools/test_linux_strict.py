"""The Linux-strict compiler wrappers reject, on a Mac, the three kinds of probe build that failed only on Linux."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
STRICT = ROOT / "tools/probekit/strict"
sys.path.insert(0, str(ROOT / "tools"))
import ci_test  # noqa: E402

pytestmark = pytest.mark.skipif(sys.platform != "darwin", reason="the wrappers imitate Linux on macOS only")

PI = "#include <math.h>\ndouble pi(void) { return M_PI; }\n"
HYPOT = "#include <math.h>\nint main(int c, char **v) { (void)v; return (int)hypotf((float)c, 4.0f); }\n"
MACH = "#include <mach/mach_time.h>\nunsigned long long now(void) { return mach_absolute_time(); }\n"
MARKED = ("#ifdef __APPLE__\n#include <mach/mach_time.h> /* apple-only */\n#endif\n"
          "#include <stdint.h>\nint zero(void) { return 0; }\n")


def compile_with(tmp_path: Path, name: str, source: str, *arguments: str, env: dict[str, str] | None = None,
                 header: str | None = None) -> subprocess.CompletedProcess[str]:
    (tmp_path / "probe.c").write_text(source)
    if header is not None:
        (tmp_path / "probe.h").write_text(header)
    environment = ci_test.strict_environment()
    assert environment is not None
    environment.update(env or {})
    return subprocess.run([name, *arguments], cwd=tmp_path, env=environment, capture_output=True, text=True)


def test_ci_test_puts_the_wrappers_first_on_path() -> None:
    environment = ci_test.strict_environment()
    assert environment is not None
    assert environment["PATH"].split(os.pathsep)[0] == str(STRICT)
    assert (STRICT / "cc").exists() and (STRICT / "clang").exists()


@pytest.mark.parametrize("name", ["cc", "clang"])
def test_iso_standard_hides_m_pi_as_glibc_does(tmp_path: Path, name: str) -> None:
    strict = compile_with(tmp_path, name, PI, "-std=c11", "-c", "probe.c", "-o", "probe.o")
    assert strict.returncode != 0
    assert "linux-strict:" in strict.stderr and "M_PI" in strict.stderr
    # A GNU standard, or the test's own feature-test macro, keeps M_PI as on Linux.
    assert compile_with(tmp_path, name, PI, "-std=gnu11", "-c", "probe.c", "-o", "probe.o").returncode == 0
    assert compile_with(tmp_path, name, "#define _DEFAULT_SOURCE\n" + PI, "-std=c11", "-c", "probe.c",
                        "-o", "probe.o").returncode == 0


def test_iso_standard_keeps_c99_library_functions(tmp_path: Path) -> None:
    source = ('#include <stdio.h>\n#include <stdarg.h>\n#include <time.h>\n'
              'int f(char *b, ...) { struct timespec t; va_list a; va_start(a, b);'
              ' int n = vsnprintf(b, 4, "%d", a) + snprintf(b, 4, "x"); va_end(a);'
              ' return n + timespec_get(&t, TIME_UTC); }\n')
    result = compile_with(tmp_path, "cc", source, "-std=c11", "-Wall", "-Werror", "-c", "probe.c", "-o", "probe.o")
    assert result.returncode == 0, result.stderr


def test_apple_only_header_is_refused_directly_and_through_a_local_header(tmp_path: Path) -> None:
    direct = compile_with(tmp_path, "clang", MACH, "-c", "probe.c", "-o", "probe.o")
    assert direct.returncode != 0
    assert "linux-strict:" in direct.stderr and "<mach/mach_time.h>" in direct.stderr
    nested = compile_with(tmp_path, "clang", '#include "probe.h"\nint zero(void) { return 0; }\n',
                          "-c", "probe.c", "-o", "probe.o", header=MACH)
    assert nested.returncode != 0
    assert "probe.h includes <mach/mach_time.h>" in nested.stderr


def test_apple_only_mark_allows_a_guarded_include(tmp_path: Path) -> None:
    result = compile_with(tmp_path, "clang", MARKED, "-c", "probe.c", "-o", "probe.o")
    assert result.returncode == 0, result.stderr
    assert not any(line.startswith(".") for line in result.stderr.splitlines())


def test_link_without_libm_is_refused(tmp_path: Path) -> None:
    missing = compile_with(tmp_path, "cc", HYPOT, "probe.c", "-o", "probe")
    assert missing.returncode != 0
    assert "linux-strict:" in missing.stderr and "hypotf" in missing.stderr and "-lm" in missing.stderr
    assert not (tmp_path / "probe").exists()
    linked = compile_with(tmp_path, "cc", HYPOT, "probe.c", "-o", "probe", "-lm")
    assert linked.returncode == 0, linked.stderr
    assert (tmp_path / "probe").exists()


def test_compiler_diagnostics_still_reach_the_test(tmp_path: Path) -> None:
    result = compile_with(tmp_path, "cc", "int f(void) { int unused; return 0; }\n", "-Wall", "-c", "probe.c",
                          "-o", "probe.o")
    assert result.returncode == 0
    assert "unused" in result.stderr
    assert "linux-strict" not in result.stderr


def test_switch_turns_the_wrappers_into_pass_throughs(tmp_path: Path) -> None:
    result = compile_with(tmp_path, "cc", MACH + HYPOT, "-std=c11", "probe.c", "-o", "probe",
                          env={"HALO_LINUX_STRICT": "0"})
    assert result.returncode == 0, result.stderr


def test_headers_the_runner_image_and_the_compiler_provide_are_allowed(tmp_path: Path) -> None:
    # zlib.h comes from the runner image's zlib1g-dev; arm_neon.h is clang's own, reached only on arm64.
    source = "#include <zlib.h>\n#ifdef __ARM_NEON\n#include <arm_neon.h>\n#endif\nint zero(void) { return 0; }\n"
    result = compile_with(tmp_path, "clang", source, "-c", "probe.c", "-o", "probe.o")
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize("output", [["-oprobe"], ["--output=probe"]])
def test_attached_output_forms_are_still_checked_for_libm(tmp_path: Path, output: list[str]) -> None:
    result = compile_with(tmp_path, "cc", HYPOT, "probe.c", *output)
    assert result.returncode != 0
    assert "hypotf" in result.stderr


def test_libm_passed_to_the_linker_counts(tmp_path: Path) -> None:
    result = compile_with(tmp_path, "cc", HYPOT, "probe.c", "-o", "probe", "-Wl,-lm")
    assert result.returncode == 0, result.stderr


def test_sanitized_clang_link_brings_libm(tmp_path: Path) -> None:
    result = compile_with(tmp_path, "clang", HYPOT, "-fsanitize=undefined", "probe.c", "-o", "probe")
    assert result.returncode == 0, result.stderr


def test_c_language_override_is_checked_whatever_the_suffix(tmp_path: Path) -> None:
    (tmp_path / "probe.m").write_text(MACH)
    environment = ci_test.strict_environment()
    assert environment is not None
    result = subprocess.run(["clang", "-x", "c", "-c", "probe.m", "-o", "probe.o"], cwd=tmp_path, env=environment,
                            capture_output=True, text=True)
    assert result.returncode != 0
    assert "<mach/mach_time.h>" in result.stderr


def test_libm_functions_arm64_inlines_still_need_lm(tmp_path: Path) -> None:
    source = ("#include <math.h>\nint main(int c, char **v) { (void)v; double x = c; float y = (float)c;"
              " x *= 0.37; return (int)(floor(x) + ceil(x) + fma(x, x, x) + (double)lround(x) + sqrtf(y)); }\n")
    for level in ("-O0", "-O2"):
        result = compile_with(tmp_path, "clang", source, level, "probe.c", "-o", "probe")
        assert result.returncode != 0, level
        for name in ("floor", "ceil", "fma", "lround", "sqrtf"):
            assert name in result.stderr, (level, name)
    assert compile_with(tmp_path, "clang", source, "-O2", "probe.c", "-o", "probe", "-lm").returncode == 0


def test_math_that_x86_64_inlines_or_folds_links_without_lm(tmp_path: Path) -> None:
    # fabs, fmin and fmax are instructions on x86-64 too, and constant arguments fold away.
    source = ("#include <math.h>\nint main(int c, char **v) { (void)v; float y = (float)c;"
              " return (int)(fabsf(y) + fmaxf(y, 1.0f) + fminf(y, 2.0f) + ceil(2.5) + sqrt(4.0)); }\n")
    result = compile_with(tmp_path, "clang", source, "-O2", "probe.c", "-o", "probe")
    assert result.returncode == 0, result.stderr


def test_combined_sine_and_cosine_needs_lm(tmp_path: Path) -> None:
    source = "#include <math.h>\nint main(int c, char **v) { (void)v; double x = c; return (int)(sin(x) * cos(x) * 9); }\n"
    result = compile_with(tmp_path, "cc", source, "-O2", "probe.c", "-o", "probe")
    assert result.returncode != 0
    assert "cos, sin" in result.stderr


def test_lm_before_the_sources_is_refused(tmp_path: Path) -> None:
    result = compile_with(tmp_path, "cc", HYPOT, "-lm", "probe.c", "-o", "probe")
    assert result.returncode != 0
    assert "-lm comes before an input file" in result.stderr
    # A separate --output value is not an input.
    assert compile_with(tmp_path, "cc", HYPOT, "probe.c", "-lm", "--output", "probe").returncode == 0


def test_a_source_that_cannot_build_for_x86_64_falls_back_to_the_arm64_check(tmp_path: Path) -> None:
    source = "#ifdef __x86_64__\n#error arm64 only\n#endif\nint main(void) { return 0; }\n"
    result = compile_with(tmp_path, "clang", source, "probe.c", "-o", "probe")
    assert result.returncode == 0, result.stderr
    assert (tmp_path / "probe").exists()
    failing = compile_with(tmp_path, "clang", "#ifdef __x86_64__\n#error arm64 only\n#endif\n" + HYPOT,
                           "probe.c", "-o", "probe")
    assert failing.returncode != 0 and "hypotf" in failing.stderr
