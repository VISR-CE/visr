"""The mixer is handed between the SDL device and the silent clock with no gap and no overlap."""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run

CASES = ["toggle-orders", "blocking-close", "no-gap", "open-failure"]
NEGATIVE_CONTROLS = {
    "no-gate-on-close": (("__atomic_store_n(&silence_gate, 1, __ATOMIC_RELEASE);\n\t\taudio_clock_start();",
                          "audio_clock_start();"), "toggle-orders"),
    "clock-not-stopped": (("audio_clock_stop();\n\t\t\taudio_driver = AUDIO_DRIVER_SDL;",
                           "audio_driver = AUDIO_DRIVER_SDL;"), "toggle-orders"),
    "no-clock-on-close": (("audio_clock_start();\n\t\taudio_driver = AUDIO_DRIVER_CLOCK;\n\t\taudio_stream = NULL;",
                           "audio_driver = AUDIO_DRIVER_CLOCK;\n\t\taudio_stream = NULL;"), "no-gap"),
}


def generated(fault=None):
    source = read("port/linux/src/dsound_sdl.c")
    start = source.index("static SDL_AudioStream *audio_stream;")
    end = source.index("static void SDLCALL audio_callback")
    code = source[start:end]
    code += "\n".join(function(source, name) for name in ["audio_callback", "silent_clock_thread",
        "audio_clock_start", "audio_clock_stop", "audio_open_device", "audio_close_device"])
    code += "\nstatic struct audio_backend audio_backend = { audio_open_device, audio_close_device };\n"
    code += function(source, "audio_set_device")
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("audio_driver", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("audio_driver", generated(fault)), case)
    assert status == CHECK_FAILED, f"{control} escaped {case}: {output}"
