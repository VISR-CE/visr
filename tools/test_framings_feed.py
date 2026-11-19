"""The framings feed pack: record validation, signing, the size limit and the Pages allow-list."""
import base64
import json

import pytest

import data_pack
import map_framing

HASH = "blake2b:" + "ab" * 32


def record(key=HASH):
    data = map_framing.import_camera("stock:example", "1 2 3\n1 0 0\n0 0 1\n1.12\n")
    entry = data["maps"].pop("stock:example")
    data["maps"][key] = entry
    return data


def test_record_accepts_hash_keys():
    assert data_pack.validate_framings_record(data_pack.canonical(record()))["maps"][HASH]


def test_empty_map_set_is_valid():
    assert data_pack.validate_framings_record(data_pack.canonical({"schema": 2, "maps": {}}))["maps"] == {}


@pytest.mark.parametrize("key", ["stock:a10", "blake2b:" + "AB" * 32, "blake2b:" + "ab" * 31, "x"])
def test_record_rejects_non_feed_keys(key):
    with pytest.raises(ValueError):
        data_pack.validate_framings_record(data_pack.canonical(record(key)))


def test_record_rejects_bad_shot_junk_duplicates_and_size():
    bad = record()
    bad["maps"][HASH]["shots"][0]["fov"] = 9
    for blob in (data_pack.canonical(bad), b"not json", b"",
                 b'{"schema":2,"maps":{},"maps":{}}', b'{"schema":2,"maps":{"a":NaN}}',
                 b" " * (data_pack.FRAMINGS_MAX_BYTES + 1)):
        with pytest.raises(ValueError):
            data_pack.validate_framings_record(blob)


def build(tmp_path, content, name="map-framings.json"):
    folder = tmp_path / "in"
    folder.mkdir()
    (folder / name).write_bytes(content)
    key = tmp_path / "seed"
    key.write_bytes(bytes(range(32)))
    key.chmod(0o600)
    out = tmp_path / "framings.json"
    data_pack.main(["--kind", "framings", "--channel", "tester", "--sequence", "1",
                    "--compatibility", "map-framings-2", "--input", str(folder),
                    "--output", str(out), "--signing-key", str(key)])
    return json.loads(out.read_text())


def test_signed_envelope_round_trip(tmp_path):
    content = data_pack.canonical(record())
    envelope = build(tmp_path, content)
    manifest = json.loads(base64.b64decode(envelope["manifest"]))
    assert manifest["kind"] == "framings" and manifest["compatibility"] == "map-framings-2"
    files = json.loads(base64.b64decode(envelope["payload"]))["files"]
    assert base64.b64decode(files["map-framings.json"]) == content


def test_pack_refuses_bad_content_and_names(tmp_path):
    with pytest.raises((SystemExit, ValueError)):
        build(tmp_path, b"{}")
    other = tmp_path / "b"
    other.mkdir()
    with pytest.raises((SystemExit, ValueError)):
        build(other, data_pack.canonical(record()), name="other.json")


def test_pages_allow_list_and_limit():
    import pages_asset_overlay as overlay
    assert overlay.allowed_asset("docs/site/packs/tester/framings.json") == ("framings", None)
    assert overlay.limit_for("docs/site/packs/tester/framings.json") == data_pack.ENVELOPE_LIMITS["framings"]
    with pytest.raises(ValueError):
        overlay.allowed_asset("docs/site/packs/tester/framings-2.json")
    with pytest.raises(ValueError):
        overlay.verify_asset("docs/site/packs/tester/framings.json", b"x" * (data_pack.ENVELOPE_LIMITS["framings"] + 1))
