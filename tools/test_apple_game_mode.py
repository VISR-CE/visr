"""Every Apple app target's Info.plist declares the games category and the Game Mode keys its platform supports."""
import plistlib

import apple_game_mode

ROOT = apple_game_mode.ROOT


def test_every_plist_matches_the_table():
    assert apple_game_mode.problems() == []


def test_table_lists_exactly_the_plists_the_build_can_pick():
    assert apple_game_mode.build_plists() == set(apple_game_mode.PLISTS)


def test_no_generated_plist_bypasses_the_table():
    for path in (ROOT / "port").rglob("*"):
        if path.suffix in {".txt", ".cmake", ".py", ".sh", ".pbxproj", ".yml"} and path.is_file():
            text = path.read_text(errors="ignore")
            assert "INFOPLIST_KEY_" not in text and "GENERATE_INFOPLIST_FILE" not in text, path


def test_integer_does_not_pass_for_a_boolean(tmp_path, monkeypatch):
    plist = tmp_path / "Info.plist"
    plist.write_bytes(plistlib.dumps({"GCSupportsGameMode": 1, "LSSupportsGameMode": True,
                                      **apple_game_mode.CATEGORY}))
    monkeypatch.setattr(apple_game_mode, "ROOT", tmp_path)
    monkeypatch.setattr(apple_game_mode, "PLISTS", {"Info.plist": apple_game_mode.PLISTS["port/ios/Info.plist"]})
    assert any("GCSupportsGameMode" in p for p in apple_game_mode.problems())
