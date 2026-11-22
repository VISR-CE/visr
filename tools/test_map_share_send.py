"""What map sharing sends to other players (port/linux/game/map_share.c, map_share_files.c, map_share_policy.c):
community maps only. Never Custom Edition's resource maps (bitmaps.map, sounds.map, loc.map) and never one of the
publisher's own maps, by any path that sends a file: the catalog every whole file is sent from, the host's offer, and
what a downloading machine passes on of its download (its state, haves, pieces' hashes, pieces and blocks).

The real code is taken from the sources (tools/harness) and compiled against the fake world in
tools/harness/tests/map_share_send.c, with the real map_share_policy.c and cache_file_formats.c beside it. It is
compiled for this machine rather than the game's 32-bit ABI, so it runs in the portable CI gate (tools/ci_test.py).
Negative controls take each gate away and expect a case to fail: a path that sends without asking the policy fails
this test.

When real Custom Edition maps are present (HALO_CUSTOM_EDITION_MAPS, or the gitignored assets/custom_edition),
test_real_maps_* also check every stock map and resource map there against the policy.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from harness import CHECK_FAILED, constant, enum_with, function, mutated, read, structure  # noqa: E402

GAME = ROOT / "port/linux/game"
FIXTURE = ROOT / "tools/harness/tests/map_share_send.c"
CASES = ["catalog", "offer", "relay", "relay-unknown"]

MAP_SHARE_FILES_FUNCTIONS = ("hex_digit", "map_share_name_hashed", "map_share_name_base_length",
                             "map_share_piece_count", "map_share_file_sendable", "map_share_file_hash",
                             "map_share_file_get")
MAP_SHARE_FUNCTIONS = ("put_long", "level_name_copy", "frame_room", "frame_begin", "frame_piece",
                       "current_files_hashed", "download_file_sendable", "sent_catalog_file", "sent_download_file",
                       "send_state", "piece_sendable", "connection_send_hashes", "serving_file_size", "block_read",
                       "download_piece_had", "host_offer", "host_request")

# a fault, the file and function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "catalog-takes-anything": (("\tif (!map_share_file_sendable(path, kind, size, NULL, 0))\n",
                                "\tif (FALSE)\n"), "map_share_files.c", "map_share_file_hash", "catalog"),
    "resource-kinds-sent": (("\tif (kind != _resource_map_none)\n\t\treturn _map_share_policy_resource_map;\n", ""),
                            "map_share_policy.c", None, "catalog"),
    "file-name-trusted": (("if (!name || publisher_name(name, name_length))", "if (!name)"),
                          "map_share_policy.c", None, "catalog"),
    "header-name-trusted": (("if (publisher_name(identity.name, (uint32_t)strlen(identity.name)) || ",
                             "if ("), "map_share_policy.c", None, "catalog"),
    "checksum-trusted": ((" || publisher_checksum(identity.checksum))", ")"), "map_share_policy.c", None,
                         "catalog"),
    "any-cache-sent": (("identity.format != _cache_file_format_custom_edition_cache", "0"), "map_share_policy.c",
                       None, "catalog"),
    "hashed-name-trusted": (("(uint32_t)map_share_name_base_length(name)", "(uint32_t)strlen(name)"),
                            "map_share_files.c", "map_share_file_sendable", "catalog"),
    "refused-file-kept": (("\t\t\tfile->ready = FALSE;\n\t\t\tfile->failed = TRUE;\n", ""), "map_share_files.c",
                          "map_share_file_hash", "catalog"),
    "offer-what-it-can": (("\t\t\tmachine->file_count = 0;\n\t\t\tbreak;\n", "\t\t\tcontinue;\n"), "map_share.c",
                          "host_request", "offer"),
    "relay-anything": (("return file && download_file_sendable(file) ? file : NULL;", "return file;"),
                       "map_share.c", "sent_download_file", "relay"),
    "relay-unjudged": (("return file->sendable == _sendable_yes;", "return file->sendable != _sendable_no;"),
                       "map_share.c", "download_file_sendable", "relay-unknown"),
    "haves-of-anything": (("index < MAXIMUM_CONNECTIONS && download_file_sendable(file)",
                           "index < MAXIMUM_CONNECTIONS"), "map_share.c", "download_piece_had", "relay"),
    "pieces-of-anything": (("struct download_file *file = sent_download_file(connection, index);\n\n\tif (catalog_file)"
                            "\n\t\treturn piece >= 0 && piece < catalog_file->piece_count;",
                            "struct download_file *file = connection->download_file[index] != NONE ? "
                            "&map_share_globals.download.files[connection->download_file[index]] : NULL;\n\n"
                            "\tif (catalog_file)\n\t\treturn piece >= 0 && piece < catalog_file->piece_count;"),
                           "map_share.c", "piece_sendable", "relay"),
}


def find_clang():
    return os.environ.get("CC") or shutil.which("clang")


def generated(fault=None, fault_file=None, faulty_function=None):
    """config.inc and under_test.inc, and the policy's source, the fault applied where it is."""
    header = read("port/linux/game/map_share.h")
    share = read("port/linux/game/map_share.c")
    files = read("port/linux/game/map_share_files.c")
    policy = read("port/linux/game/map_share_policy.c")
    config = "#define P2P_MAP_SHARE_PORT %d\n" % constant(read("port/linux/src/p2p.h"), "P2P_MAP_SHARE_PORT")
    config += "#define P2P_PLAYER_KEY_HASH_SIZE %d\n" % constant(read("port/linux/src/p2p.h"),
                                                                 "P2P_PLAYER_KEY_HASH_SIZE")
    config += "#define HALO_PORT_MAXIMUM_NETWORK_MACHINES %d\n" % constant(
        read("port/linux/include/halo_port_limits.h"), "HALO_PORT_MAXIMUM_NETWORK_MACHINES")
    config += enum_with(header, "MAP_SHARE_HASH_BYTES") + "\n"
    config += structure(header, "map_share_file") + "\n"
    for member in ("MAXIMUM_SOURCES", "_frame_hello", "_have_none", "_offer_ok", "_offer_in_progress_bit",
                   "_download_transferring", "_file_awaiting_hashes", "_sendable_unknown", "_connection_open"):
        config += enum_with(share, member) + "\n"
    for name in ("distributed_map_share_request", "distributed_map_share_offer", "distributed_map_share_offer_file",
                 "download_file", "source", "download", "request", "serving", "connection", "host_machine",
                 "map_share_globals"):
        config += structure(share, name) + "\n"
    for member in ("MAXIMUM_SHARE_FILES", "_hash_job_file"):
        config += enum_with(files, member) + "\n"
    for name in ("hash_job", "naming", "known_hash", "map_share_files_globals"):
        config += structure(files, name) + "\n"
    text = ""
    for source, file_name, names in ((files, "map_share_files.c", MAP_SHARE_FILES_FUNCTIONS),
                                     (share, "map_share.c", MAP_SHARE_FUNCTIONS)):
        for name in names:
            code = function(source, name)
            if fault and fault_file == file_name and name == faulty_function:
                code = mutated(code, *fault)
            text += code + "\n"
    if fault and fault_file == "map_share_policy.c":
        policy = mutated(policy, *fault)
    return config, text, policy


def build(work, fault=None, fault_file=None, faulty_function=None):
    clang = find_clang()
    if clang is None:
        pytest.skip("clang is needed")
    config, text, policy = generated(fault, fault_file, faulty_function)
    (work / "config.inc").write_text(config)
    (work / "under_test.inc").write_text(text)
    (work / "map_share_policy.c").write_text(policy)
    executable = work / "map_share_send"
    command = [clang, "-std=gnu99", "-O1", "-g", "-Wall", "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
               "-I", str(ROOT / "tools/harness/include"), "-I", str(work), "-I", str(GAME), str(FIXTURE),
               str(work / "map_share_policy.c"), str(GAME / "cache_file_formats.c"), "-o", str(executable)]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        pytest.fail(f"map_share_send.c does not compile:\n{result.stderr}")
    return executable


def run(executable, case):
    result = subprocess.run([str(executable), case], capture_output=True, text=True, timeout=120)
    return result.returncode, (result.stdout + result.stderr).strip()


@pytest.fixture(scope="module")
def executable(tmp_path_factory):
    return build(tmp_path_factory.mktemp("map-share-send"))


@pytest.mark.parametrize("case", CASES)
def test_case(executable, case):
    status, output = run(executable, case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control, tmp_path):
    fault, fault_file, faulty_function, case = NEGATIVE_CONTROLS[control]
    status, output = run(build(tmp_path, fault, fault_file, faulty_function), case)
    assert status == CHECK_FAILED, f"{fault_file} with a fault ({control}) passed '{case}': the test cannot see it"


# ---------- the publisher's names


def policy_names():
    policy = read("port/linux/game/map_share_policy.c")
    start = policy.index("publisher_names[] =")
    table = policy[policy.index("{", start):policy.index("};", start)]
    return {name.strip().strip('"') for name in table.strip("{} \n\t").replace("\n", " ").split(",") if name.strip()}


# Custom Edition's own maps and resource maps, Halo PC's campaign levels: the names the policy must know, written out
# here rather than taken from the code under test
PUBLISHER_FILES = {
    "beavercreek", "bloodgulch", "boardingaction", "carousel", "chillout", "damnation", "dangercanyon", "deathisland",
    "gephyrophobia", "hangemhigh", "icefields", "infinity", "longest", "prisoner", "putput", "ratrace", "sidewinder",
    "timberland", "wizard", "ui", "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40", "bitmaps",
    "sounds", "loc",
}


def test_policy_knows_every_publisher_file():
    assert PUBLISHER_FILES <= policy_names()


def test_policy_knows_the_menus_stock_maps():
    """custom_edition_maps.c lists Halo PC's own maps as stock: each is a publisher's map to map sharing too."""
    maps = read("port/linux/game/custom_edition_maps.c")
    start = maps.index("} const stock_maps[] =")
    table = maps[start:maps.index("};", start)]
    names = {part.split('"')[1] for part in table.split("{ ")[1:]}
    assert names and names <= policy_names()


# ---------- real maps, when present (never committed)


def real_maps_directory():
    configured = os.environ.get("HALO_CUSTOM_EDITION_MAPS")
    folder = Path(configured) if configured else ROOT / "assets" / "custom_edition"
    return folder if (folder / "bitmaps.map").is_file() else None


@pytest.fixture(scope="module")
def real_maps():
    folder = real_maps_directory()
    if folder is None:
        pytest.skip("no Custom Edition maps: set HALO_CUSTOM_EDITION_MAPS or extract them to assets/custom_edition")
    return folder


def test_real_maps_none_is_sent_under_any_name(real_maps, tmp_path):
    """Every stock map and resource map there, under its own name and renamed, is refused."""
    clang = find_clang()
    if clang is None:
        pytest.skip("clang is needed")
    driver = tmp_path / "judge.c"
    driver.write_text(r'''
#include "map_share_policy.h"
#include "cache_file_formats.h"
#include <stdio.h>
#include <string.h>
int main(int count, char **arguments)
{
	int index;
	for (index = 2; index < count; index++)
	{
		unsigned char header[CACHE_FILE_HEADER_BYTES] = {0};
		FILE *stream = fopen(arguments[index], "rb");
		long size;
		size_t bytes;
		if (!stream)
			return 2;
		bytes = fread(header, 1, sizeof(header), stream);
		fseek(stream, 0, SEEK_END);
		size = ftell(stream);
		fclose(stream);
		printf("%d\n", map_share_policy_may_send(map_share_policy_judge(0, arguments[1], (unsigned)strlen(arguments[1]),
			header, (unsigned)bytes, (unsigned)size)));
	}
	return 0;
}
''')
    judge = tmp_path / "judge"
    subprocess.run([clang, "-std=c99", "-I", str(GAME), str(driver), str(GAME / "map_share_policy.c"),
                    str(GAME / "cache_file_formats.c"), "-o", str(judge)], check=True)
    files = sorted(path for path in real_maps.glob("*.map") if path.stem in PUBLISHER_FILES)
    assert files
    for name in ("hugeass", *(path.stem for path in files)):
        result = subprocess.run([str(judge), name, *map(str, files)], capture_output=True, text=True, check=True)
        assert set(result.stdout.split()) == {"0"}, f"a publisher's file is sent as {name}.map"
