"""tools/replay_check.py's drop-in route: the unsigned local catalog it writes passes the app's catalog rules."""
from pathlib import Path
import argparse
import json
import plistlib
import struct
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import benchmark_catalog  # noqa: E402
import replay_check  # noqa: E402

ROUTE = """map = b30
difficulty = normal
entered = start
first_tick = 0
random_seed = 23df5293
players = 1
last_tick = 150
seconds = 5.0
"""


def take(folder):
    base = folder / "take-b30-20261020-000000"
    moving = struct.unpack("<I", struct.pack("<f", 0.5))[0]
    actions = ["halo action recording 1"] + [f"0 {tick} 00000000 00000000 00000000 {moving:08x} 00000000 00000000 "
                                             "ffffffff 0000ffff" for tick in range(150)]
    base.with_suffix(".actions").write_text("\n".join(actions) + "\n")
    base.with_suffix(".input").write_text("input\n")
    base.with_suffix(".seeds").write_text("seeds\n")
    base.with_suffix(".route").write_text(ROUTE)
    return base


def test_local_catalog_loads(tmp_path):
    local = tmp_path / "local-routes"
    replay_check.write_local_routes(take(tmp_path), local, "check")
    [case] = benchmark_catalog.load_catalog(local)
    assert (case["id"], case["kind"], case["map"], case["recording"], case["lead"], case["window"]) == \
        ("check", "gameplay", "b30", "check", 0, 4)
    assert "window = 4" in (local / "check.route").read_text()
    assert sorted(path.name for path in local.iterdir()) == [
        "benchmark-catalog.json", "check.actions", "check.input", "check.route", "check.seeds"]


def test_local_catalog_repeats_the_takes_enemy_multiplier(tmp_path):
    folder = take(tmp_path)
    folder.with_suffix(".route").write_text(ROUTE + "enemy_multiplier = 2.000\n")
    local = tmp_path / "local-routes"
    replay_check.write_local_routes(folder, local, "check")
    [case] = benchmark_catalog.load_catalog(local)
    assert case["enemy_multiplier"] == 2.0
    assert "enemy_multiplier = 2.000" in (local / "check.route").read_text()


@pytest.mark.parametrize("preset,window", [("a50-hangar", "a50-hangar"), ("b40-exterior-a", "b40-exta")])
def test_staging_presets_match_battle_commands(preset, window):
    import tomllib

    windows = tomllib.loads((replay_check.ROOT / "tools/perf_compare/windows.toml").read_text())
    commands = []
    steps = json.loads(replay_check.staging_json(json.dumps(replay_check.STAGING_PRESETS[preset])))
    for step in steps[:-1]:
        key, value = next(iter(step.items()))
        if key == "teleport_player":
            commands.append(f"(object_teleport (unit (list_get (players) 0)) {value})")
        elif key == "ai_see":
            commands.append(f"(ai_magically_see_encounter {' '.join(value)})")
        else:
            command = "ai_magically_see_players" if key == "ai_see_player" else key
            commands.append(f"({command} {value})")
    assert commands == windows[window]["commands"]
    assert steps[-1] == {"deathless": True}


@pytest.mark.parametrize("preset", replay_check.STAGING_PRESETS)
def test_local_catalog_repeats_staging(tmp_path, preset):
    base = take(tmp_path)
    steps = replay_check.STAGING_PRESETS[preset]
    canonical = replay_check.staging_json(json.dumps(steps, indent=2))
    base.with_suffix(".route").write_text(ROUTE + f"staging = {canonical}\nstaging_tick = 31\nstaging_segment = 0\n")
    local = tmp_path / "local-routes"
    replay_check.write_local_routes(base, local, "check")
    [case] = benchmark_catalog.load_catalog(local)
    assert case["staging"] == steps
    assert f"staging = {canonical}\nstaging_tick = 31\nstaging_segment = 0\n" in (local / "check.route").read_text()


@pytest.mark.parametrize("text", ['[]', '[{"deathless":false}]', '[{"ai_place":"bad-name"}]',
                                  '[{"switch_bsp":32}]', '[{"deathless":true,"deathless":true}]', 'invalid'])
def test_staging_argument_rejects_invalid_steps(text):
    with pytest.raises(argparse.ArgumentTypeError):
        replay_check.staging_json(text)


@pytest.mark.parametrize("option,value", [("--staging", '[{"deathless": true}]'),
                                          ("--staging-preset", "a50-hangar")])
def test_recording_receives_staging_as_a_toml_string(tmp_path, monkeypatch, option, value):
    import tomllib

    app = tmp_path / "VISR.app"
    (app / "Contents").mkdir(parents=True)
    (app / "Contents/Info.plist").write_bytes(plistlib.dumps({"CFBundleIdentifier": "test.runner"}))
    monkeypatch.setattr(replay_check.mac_run, "native_app_default", lambda: app)
    monkeypatch.setattr(replay_check.mac_run, "native_data_folder", lambda _: tmp_path / "data")
    monkeypatch.setattr(sys, "argv", ["replay_check.py", "--out", str(tmp_path / "out"), option, value])
    calls = []
    monkeypatch.setattr(replay_check, "runner", lambda *args: calls.append(args) or 0)
    assert replay_check.main() == 2  # The stub runner leaves no take.
    [setting] = [arg for arg in calls[0] if isinstance(arg, str) and arg.startswith("debug.benchmark_staging=")]
    recorded = json.loads(tomllib.loads(setting)["debug"]["benchmark_staging"])
    assert recorded == (json.loads(value) if option == "--staging" else replay_check.STAGING_PRESETS[value])


def test_local_catalog_compiled_out_without_routes():
    source = (Path(replay_check.ROOT) / "port/ios/host/host_benchmark.m").read_text()
    start = source.index("static NSString *local_catalog_folder(void) {")
    body = source[start:source.index("\n}\n", start)]
    assert "#if HALO_BENCHMARK_NO_ROUTES\n    return nil;\n#else" in body
    assert 'updates[@"benchmark_catalog_status"] = @"local_unsigned";' in source
    assert 'record[@"catalog_signed"] = @(catalog_signed());' in source


def test_suite_launch_keeps_local_routes(tmp_path):
    import mac_run

    folder = tmp_path / "benchmark"
    (folder / "local-routes").mkdir(parents=True)
    (folder / "local-routes" / "benchmark-catalog.json").write_text("{}")
    (folder / "recordings").mkdir()
    (folder / "journal.txt").write_text("old")
    mac_run.clear_benchmark_folder(folder)
    assert [path.name for path in folder.iterdir()] == ["local-routes"]
    assert (folder / "local-routes" / "benchmark-catalog.json").is_file()
