"""What map sharing sends to other players (port/linux/game/map_share.c, map_share_files.c, map_share_policy.c):
community maps only. Never Custom Edition's resource maps (bitmaps.map, sounds.map, loc.map) and never one of its
stock maps (known by content hash, everything but the cache header, so an edited header checksum does not hide one:
port/runtime/stock_maps.h), by any path that sends a file: the catalog every whole file is sent from, the host's
offer, and what a downloading machine passes on of its download (its state, haves, pieces' hashes, pieces and blocks).

The real code is taken from the sources (tools/harness) and compiled against the fake world in
tools/harness/tests/map_share_send.c, with the real map_share_policy.c and cache_file_formats.c beside it. It is
compiled for this machine rather than the game's 32-bit ABI, so it runs in the portable CI gate (tools/ci_test.py).
Negative controls take each gate away and expect a case to fail: a path that sends without asking the policy fails
this test.

When real Custom Edition maps are present (HALO_CUSTOM_EDITION_MAPS, or the gitignored assets/custom_edition),
test_real_maps_* also check every stock map and resource map there against the policy.
"""

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from harness import CHECK_FAILED, constant, enum_with, function, mutated, read, structure  # noqa: E402

GAME = ROOT / "port/linux/game"
MONOCYPHER = ROOT / "port/third_party/monocypher"
FIXTURE = ROOT / "tools/harness/tests/map_share_send.c"
CASES = ["catalog", "catalog-once", "catalog-changing", "catalog-replaced", "downloaded", "offer", "relay", "relay-unknown", "relay-refused", "relay-tag-range"]

MAP_SHARE_FILES_FUNCTIONS = ("hex_digit", "map_share_name_hashed", "map_share_name_base_length",
                             "map_share_piece_count", "map_share_file_get", "map_share_file_hash_of_pieces",
                             "map_share_first_piece_rest_hash", "map_share_content_hash", "file_source_read",
                             "map_share_campaign_read", "map_share_tag_data_pieces", "map_share_content_read",
                             "hash_job_run", "map_share_file_judge", "map_share_file_unchanged", "known_hash_find",
                             "map_share_map_hash_remember", "map_share_file_hash", "catalog_file_judged",
                             "catalog_job_finish", "map_share_file_hashed")
MAP_SHARE_FUNCTIONS = ("put_long", "level_name_copy", "frame_room", "frame_begin", "frame_piece",
                       "current_files_hashed", "download_file_has_pieces", "download_file_sendable", "sent_catalog_file", "sent_download_file",
                       "send_state", "piece_sendable", "connection_send_hashes", "serving_file_size", "block_read",
                       "download_piece_had", "host_offer", "host_files_failed", "host_request")

# a fault, the file and function it is in, and the case that must catch it
NEGATIVE_CONTROLS = {
    "catalog-takes-anything": (("\tif (!map_share_policy_may_hash(", "\tif (FALSE && !map_share_policy_may_hash("),
                               "map_share_files.c", "map_share_file_hash", "catalog"),
    "resource-kinds-sent": (("if (kind != _resource_map_none || ", "if ("), "map_share_policy.c", None, "catalog"),
    "resource-names-sent": ((" || resource_map_name(name, name_length))", ")"), "map_share_policy.c", None,
                            "catalog"),
    "content-trusted": (("if (stock_map_with_content(content))", "if (0)"), "map_share_policy.c", None, "catalog"),
    "stock-names-refused": (("if (stock_map_with_content(content))",
                             "if (stock_map_with_content(content) || "
                             "stock_map_named(identity.name, strlen(identity.name)))"),
                            "map_share_policy.c", None, "catalog"),
    "header-trusted": (("map_share_first_piece_rest_hash(piece, bytes, rest_hash);",
                        "p2p_blake2b_256(piece, bytes, rest_hash);"), "map_share_files.c",
                       "map_share_content_read", "relay"),
    "header-hashed": (("p2p_blake2b_256(piece + MIN(bytes, CACHE_FILE_HEADER_BYTES), "
                       "bytes - MIN(bytes, CACHE_FILE_HEADER_BYTES), rest_hash);",
                       "p2p_blake2b_256(piece, bytes, rest_hash);"), "map_share_files.c",
                      "map_share_first_piece_rest_hash", "catalog"),
    "sent-before-judged": (("\tfile->ready = map_share_policy_may_send(", "\tfile->ready = TRUE || map_share_policy_may_send("),
                           "map_share_files.c", "catalog_file_judged", "catalog"),
    "seeded-unjudged": (("\t\tcatalog_file_judged(file, content, campaign);\n", "\t\tfile->ready = TRUE + 0 * campaign;\n"), "map_share_files.c",
                        "map_share_file_hashed", "downloaded"),
    "read-while-changing": (("if (job->failed || file->failed || !job->modified)", "if (job->failed || file->failed)"),
                            "map_share_files.c", "catalog_job_finish", "catalog-changing"),
    "campaign-trusted": (("\tif (campaign)\n\t\treturn _map_share_policy_campaign_map;\n", ""), "map_share_policy.c",
                         None, "catalog"),
    "campaign-by-any-tag": (("if ((is_scenario || group == STRUCTURE_BSP_GROUP_TAG) && campaign_level_path(name))",
                             "if (campaign_level_path(name))"), "map_share_policy.c", None, "catalog"),
    "campaign-scenario-only": (("(is_scenario || group == STRUCTURE_BSP_GROUP_TAG)", "is_scenario"),
                               "map_share_policy.c", None, "catalog"),
    "campaign-case-sensitive": (("\t\t\tif (character >= 'A' && character <= 'Z')\n\t\t\t\tcharacter = (char)(character - 'A' + 'a');"
                                 "\n\t\t\tif (character != expected)",
                                 "\t\t\tif (character != expected)"), "map_share_policy.c", None, "catalog"),
    "campaign-by-prefix": ((" && path[position] == '\\\\')", ")"), "map_share_policy.c", None, "catalog"),
    "relay-before-tags": (("\t\tdownload_file_has_pieces(file, first_tag_piece, last_tag_piece)) &&",
                           "\t\tTRUE) &&"), "map_share.c", "download_file_sendable", "relay-unknown"),
    "relay-campaign-ignored": (("map_share_campaign_read(file->stream, file->size)", "0"), "map_share.c",
                               "download_file_sendable", "relay-refused"),
    "campaign-any-type": (("\tif (!campaign_tags_read(&identity))\n\t\treturn 0;\n", ""), "map_share_policy.c", None,
                          "catalog"),
    "solo-refused": (("\treturn cache_file_tag_names(source, campaign_tag_visit, &campaign) == _cache_file_status_ok || "
                      "campaign ?\n\t\tcampaign : -1;", "\treturn 1 + 0 * campaign;"), "map_share_policy.c", None,
                     "catalog"),
    "unreadable-tags-sent": (("== _cache_file_status_ok || campaign ?\n\t\tcampaign : -1;",
                              "== _cache_file_status_ok || campaign ?\n\t\tcampaign : 0;"), "map_share_policy.c", None,
                             "catalog"),
    "unreadable-tags-relayed": (("== _cache_file_status_ok || campaign ?\n\t\tcampaign : -1;",
                                 "== _cache_file_status_ok || campaign ?\n\t\tcampaign : 0;"), "map_share_policy.c",
                                None, "relay-refused"),
    "unreadable-silent": (("\telse if (content && campaign < 0)\n", "\telse if (0 && campaign < 0)\n"),
                          "map_share_files.c", "map_share_file_judge", "catalog"),
    "multiplayer-waits-for-tags": (("||\n\t\t!campaign_tags_read(&identity))", ")"), "map_share_policy.c", None,
                                   "relay-unknown"),
    "tag-range-last-is-first": (("*last_piece = (long)((offset + bytes - 1) / MAP_SHARE_PIECE_BYTES);",
                                 "*last_piece = *first_piece;"), "map_share_files.c", "map_share_tag_data_pieces",
                                "relay-tag-range"),
    "tag-range-first-from-last": (("*first_piece = (long)(offset / MAP_SHARE_PIECE_BYTES);",
                                   "*first_piece = (long)((offset + bytes - 1) / MAP_SHARE_PIECE_BYTES);"),
                                  "map_share_files.c", "map_share_tag_data_pieces", "relay-tag-range"),
    "content-forgotten": (("\t\tknown->content_known = campaign >= 0;\n", ""), "map_share_files.c", "catalog_file_judged",
                          "catalog-once"),
    "any-cache-sent": (("if (status != _cache_file_status_ok || identity.format != _cache_file_format_custom_edition_cache)",
                        "if (status != _cache_file_status_ok)"), "map_share_policy.c", None, "catalog"),
    "hashed-name-trusted": (("(uint32_t)map_share_name_base_length(name)", "(uint32_t)strlen(name)"),
                            "map_share_files.c", "map_share_file_judge", "catalog"),
    "refused-file-kept": (("\t\t\tfile->ready = FALSE;\n\t\t\tfile->failed = TRUE;\n", ""), "map_share_files.c",
                          "map_share_file_hash", "catalog"),
    "offer-refused-ignored": (("if (!file || file->failed)", "if (!file)"), "map_share.c", "host_files_failed",
                              "offer"),
    "offer-what-it-can": (("\t\t\tmachine->file_count = 0;\n\t\t\tbreak;\n", "\t\t\tcontinue;\n"), "map_share.c",
                          "host_request", "offer"),
    "relay-anything": (("return file && download_file_sendable(file) ? file : NULL;", "return file;"),
                       "map_share.c", "sent_download_file", "relay"),
    "relay-unjudged": (("return file->sendable == _sendable_yes;", "return file->sendable != _sendable_no;"),
                       "map_share.c", "download_file_sendable", "relay-unknown"),
    "haves-of-anything": (("index < MAXIMUM_CONNECTIONS && download_file_sendable(file)",
                           "index < MAXIMUM_CONNECTIONS"), "map_share.c", "download_piece_had", "relay"),
    "catalog-trusted-after-judging": (("map_share_sending() && map_share_file_unchanged(file)", "map_share_sending()"),
                                      "map_share.c", "sent_catalog_file", "catalog-replaced"),
    "catalog-sent-unhashed": (("return file && file->ready && map_share_sending()", "return file && map_share_sending()"),
                              "map_share.c", "sent_catalog_file", "catalog-replaced"),
    "refused-says-none": (("\t\tbody[1] = _have_pieces;\n\t\tcsmemset(body + 2, 0, bytes);",
                           "\t\tbody[1] = _have_none;\n\t\tcsmemset(body + 2, 0, bytes);"),
                          "map_share.c", "send_state", "relay"),
    "pieces-of-anything": (("struct download_file *file = sent_download_file(connection, index);\n\n\tif (catalog_file)"
                            "\n\t\treturn piece >= 0 && piece < catalog_file->piece_count;",
                            "struct download_file *file = connection->download_file[index] != NONE ? "
                            "&map_share_globals.download.files[connection->download_file[index]] : NULL;\n\n"
                            "\tif (catalog_file)\n\t\treturn piece >= 0 && piece < catalog_file->piece_count;"),
                           "map_share.c", "piece_sendable", "relay"),
}


# ---------- content hashes (map_share_content_hash), and the fixture's stock maps


PIECE_BYTES = 0x40000
HEADER_BYTES = 0x800
CONTENT_LABEL = b"VISR map content 1"
# the fixture's files' size (map_share_send.c FAKE_FILE_BYTES)
FAKE_FILE_BYTES = PIECE_BYTES + 0x1000


def blake(data):
    return hashlib.blake2b(data, digest_size=32).digest()


def content_hash(data):
    """everything of a map but its cache header, as map_share_content_hash has it"""
    parts = [blake(data[HEADER_BYTES:PIECE_BYTES])]
    parts += [blake(data[start:start + PIECE_BYTES]) for start in range(PIECE_BYTES, len(data), PIECE_BYTES)]
    return blake(CONTENT_LABEL + len(data).to_bytes(4, "little") + b"".join(parts)).hex()


def fixture_stock_table(contents):
    """the stock table, each Custom Edition map's content hash its fake copy's in the fixture (a file cannot be
    made to have the real one's)"""
    return re.sub(r'(\{ "(\w+)", "[^"]*", [\w |]+, )"[0-9a-f]{64}"',
                  lambda match: '%s"%s"' % (match.group(1), contents[match.group(2)]),
                  read("port/runtime/stock_maps.h"))


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
    for label in ("file_hash_label", "content_hash_label"):
        config += re.search(r"static char const %s\[\] = .*\n#define \w+ \d+\n" % label, files).group(0)
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


def compile_fixture(work, config, text, policy, table):
    clang = find_clang()
    if clang is None:
        pytest.skip("clang is needed")
    (work / "config.inc").write_text(config)
    (work / "under_test.inc").write_text(text)
    # (the policy where the game has it, beside the stock table it includes)
    (work / "port/linux/game").mkdir(parents=True, exist_ok=True)
    (work / "port/runtime").mkdir(parents=True, exist_ok=True)
    (work / "port/linux/game/map_share_policy.c").write_text(policy)
    (work / "port/runtime/stock_maps.h").write_text(table)
    executable = work / "map_share_send"
    command = [clang, "-std=gnu99", "-O1", "-g", "-Wall", "-Werror", "-Wno-unused-function", "-Wno-unused-variable",
               "-I", str(ROOT / "tools/harness/include"), "-I", str(work), "-I", str(work / "port/runtime"),
               "-I", str(GAME), "-I", str(MONOCYPHER), str(FIXTURE), str(work / "port/linux/game/map_share_policy.c"),
               str(GAME / "cache_file_formats.c"), str(MONOCYPHER / "monocypher.c"), "-o", str(executable)]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        pytest.fail(f"map_share_send.c does not compile:\n{result.stderr}")
    return executable


FIXTURE_CONTENTS = {}


def fixture_contents():
    """each stock map's fake copy's content hash, from the fixture's `contents` (the code as it is, no fault)"""
    if not FIXTURE_CONTENTS:
        work = Path(tempfile.mkdtemp(prefix="map-share-contents-"))
        try:
            executable = compile_fixture(work, *generated(), read("port/runtime/stock_maps.h"))
            status, output = run(executable, "contents")
            assert status == 0, output
            FIXTURE_CONTENTS.update(line.split() for line in output.splitlines())
        finally:
            shutil.rmtree(work, ignore_errors=True)
    assert set(FIXTURE_CONTENTS) == CUSTOM_EDITION_STOCK
    return FIXTURE_CONTENTS


def build(work, fault=None, fault_file=None, faulty_function=None):
    return compile_fixture(work, *generated(fault, fault_file, faulty_function),
                           fixture_stock_table(fixture_contents()))


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


# ---------- one table of the game's own maps


# Custom Edition's stock maps and the content hash of each one's file (version 609, as installed): written out here
# rather than taken from the code under test, so a mistyped hash in the table fails
CUSTOM_EDITION_CONTENT = {
    "ui": "fbc9f0191e4e04994f4d7e2e2a604d62709a26fc2dd3f0270346b9d1391a9f14",
    "beavercreek": "a65b47047a758d5591ac0422ba8489827250914f3e874d33da213564497b9dfc",
    "sidewinder": "e45682925a5e9284e4411ef5763fd186e4ca9d6839312dc8d97b1becfce142c8",
    "damnation": "8565e9b2f5eafeb2b0943c58879c7fb01cb5177856a78cecae7f61fc42591422",
    "ratrace": "1ed5b576593613fbe029dc67bc53ff522470be716bc630df741b876bc4b03f76",
    "prisoner": "2dbef4690f51a247b6ffda28c4a2dd663380da318cd9c8b5ad57600db0c86288",
    "hangemhigh": "41ce67fda4a6d4079f1961176d41004f7e128738dd2ee36257debc8a4adbcf4f",
    "chillout": "6fd4328430b4c46018012da9a723a7034d4d807e0878af581882626b70c9896f",
    "carousel": "c964a77b7931963c24444837786346c342cc1fb7394494a4c4563c09f40b0718",
    "boardingaction": "e94cd50ad600d60ff14a153ab3211db2b6ec0e4d0984e3755254279acea09894",
    "bloodgulch": "ce71cd19aa2582263d49d78d505a9ea966a51d27f547dd37094541dbdd233eab",
    "wizard": "923476273cd95271f919501f1d0c5399869b947457ba3ec164e250e6129755f3",
    "putput": "6b91613495347fd6fc82b7a7a0e8266d9f11f2c4799f8a774dec0e637bacbd11",
    "longest": "4118e66efe6516f145a59456036c0a758632b0411740b638af12f3f27ea4c208",
    "icefields": "fe134e8df69b287774f2cb9900d23089acecb29716aad5e64501c8058cfd81c6",
    "deathisland": "8a51bd63db267a955a2fa17c1f2ede550e726f07ce9c88ad8cd359652d155abb",
    "dangercanyon": "e7a3d638c2f857771a805445d5c35ad19f02f7ec0ec484f72a3147de0e685ac3",
    "infinity": "9f3c1bf35388a4b6fbab8c17a2e26acaee0e4ef3a664c8799834ee733d326dd8",
    "timberland": "7b0614d8877c2ff8a7985a9239cd5142ff3bcc3c60c77d1a1e53fd526a1aea03",
    "gephyrophobia": "39f06b3c11663169e758a66137ca2a78894e10519956848ab103e9aef9293337",
}
CUSTOM_EDITION_STOCK = set(CUSTOM_EDITION_CONTENT)
CAMPAIGN = {"a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40"}
STOCK_NAMES = CUSTOM_EDITION_STOCK | CAMPAIGN


def stock_table_rows():
    """(file, menu name, content hash or None) for each row of port/runtime/stock_maps.h"""
    rows = re.findall(r'\{ "(\w+)", "([^"]*)", [\w |]+, ("[0-9a-f]{64}"|NULL), (-?\d+) \}',
                      read("port/runtime/stock_maps.h"))
    return [(name, display, None if value == "NULL" else value.strip('"')) for name, display, value, _ in rows]


def test_stock_table_has_every_custom_edition_content_hash():
    contents = {name: content for name, _, content in stock_table_rows()}
    assert set(contents) == STOCK_NAMES
    assert {name: value for name, value in contents.items() if value} == CUSTOM_EDITION_CONTENT


def test_content_hash_leaves_the_header_out():
    """the Python content hash the pins and the fixture use agrees with itself on what counts"""
    data = bytearray(FAKE_FILE_BYTES)
    data[HEADER_BYTES] = 1
    edited = bytearray(data)
    edited[0x64:0x68] = b"\x12\x34\x56\x78"
    assert content_hash(bytes(data)) == content_hash(bytes(edited))
    edited[HEADER_BYTES + 1] = 1
    assert content_hash(bytes(data)) != content_hash(bytes(edited))


# the native sources allowed a literal list of the game's own maps: the table itself, and tests and data keyed by
# something else (the official shader lists, by hash)
STOCK_LIST_ALLOWED = {"port/runtime/stock_maps.h", "port/ios/host/host_shader_official_maps.h"}


def test_no_second_stock_table():
    """A list of the game's own maps is read from port/runtime/stock_maps.h, not written out again."""
    import re
    import subprocess as process
    files = process.run(["git", "ls-files", "port/*.c", "port/*.h", "port/*.m", "port/*.inc"], cwd=ROOT,
                        capture_output=True, text=True, check=True).stdout.split()
    lists = []
    for relative in files:
        if relative in STOCK_LIST_ALLOWED or "/tests/" in relative or relative.endswith("_test.c"):
            continue
        text = (ROOT / relative).read_text(encoding="latin-1")
        found = {name for name in re.findall(r'@?"(\w+)"', text) if name in STOCK_NAMES}
        if len(found) >= 3:
            lists.append(f"{relative}: {sorted(found)}")
    assert not lists, "stock map names written out again:\n" + "\n".join(lists)


# ---------- every path that sends takes its file from the gate


def test_only_judging_makes_a_catalog_file_ready():
    """A catalog file is sent once it is ready, and it becomes ready only where its content is judged."""
    readied = {}
    for relative in ("port/linux/game/map_share_files.c", "port/linux/game/map_share.c"):
        source = read(relative)
        for match in re.finditer(r"^(?:static )?[\w *]+?\b(\w+)\(\s*[^;{]*\)\s*\{", source, re.M):
            body = function(source, match.group(1))
            if re.search(r"->ready\s*=(?!\s*FALSE\b)", body):
                readied.setdefault(relative, set()).add(match.group(1))
    assert readied == {"port/linux/game/map_share_files.c": {"catalog_file_judged"}}, readied


# the functions of map_share.c that may read a connection's files directly: the gate itself, matching, and those
# that receive (a new function that sends must take its file from sent_catalog_file or sent_download_file)
DIRECT_FILE_READERS = {
    "sent_catalog_file", "sent_download_file", "send_state", "connection_match_files", "connection_set_files",
    "connection_new", "connection_free", "connection_close", "remote_piece_had", "remote_pieces_forget", "download_piece_had",
    "piece_choose", "download_unasked_pieces", "connection_ask", "request_remove", "frame_state", "frame_have",
    "frame_block", "frame_refuse", "frame_hashes_request", "frame_hashes", "connection_read", "connections_update",
    "download_update", "hashes_source", "download_end",
}


def test_no_path_sends_around_the_gate():
    import re
    source = read("port/linux/game/map_share.c")
    readers = set()
    for match in re.finditer(r"^(?:static )?[\w *]+?\b(\w+)\(\s*[^;{]*\)\s*\{", source, re.M):
        body = function(source, match.group(1))
        if "connection->download_file[" in body or "map_share_file_get(connection->catalog_file" in body:
            readers.add(match.group(1))
    assert readers <= DIRECT_FILE_READERS, f"read a connection's files around the gate: {readers - DIRECT_FILE_READERS}"


def call_arguments(source, callee):
    """The top-level arguments of each call of `callee` in `source` (not its definition)."""
    calls = []
    for match in re.finditer(rf"\b{callee}\(", source):
        line_start = source.rfind("\n", 0, match.start()) + 1
        if source[line_start:match.start()].startswith(("enum ", "static ")):
            continue
        depth, position, arguments, start = 0, match.end(), [], match.end()
        while True:
            character = source[position]
            if character in "([":
                depth += 1
            elif character in ")]":
                if depth == 0:
                    arguments.append(source[start:position].strip())
                    break
                depth -= 1
            elif character == "," and depth == 0:
                arguments.append(source[start:position].strip())
                start = position + 1
            position += 1
        calls.append(arguments)
    return calls


def test_no_judge_is_told_a_map_is_no_conversion():
    """Every judgment of a file is given the campaign answer read from its tags (or -1, unknown, which refuses it),
    never a constant that says it is not a conversion: on the relay path as on the catalog's."""
    calls = []
    for name in ("map_share.c", "map_share_files.c"):
        calls += [(name, arguments) for arguments in call_arguments(read(f"port/linux/game/{name}"),
                                                                   "map_share_file_judge")]
    assert {name for name, _ in calls} == {"map_share.c", "map_share_files.c"}, calls
    for name, arguments in calls:
        assert len(arguments) == 7, (name, arguments)
        assert not re.fullmatch(r"\(?(?:\(\w+\)\s*)?\d+L?\)?", arguments[-1]), (name, arguments)


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
    """Every stock map and resource map there, under its own name and renamed, each stock map with an edited header
    checksum, and a singleplayer campaign conversion made from each multiplayer map, is refused; the same copy left a
    multiplayer map is sent; the content hashes are the table's."""
    clang = find_clang()
    if clang is None:
        pytest.skip("clang is needed")
    driver = tmp_path / "judge.c"
    driver.write_text(r'''
#include "map_share_policy.h"
#include "cache_file_formats.h"
#include <stdio.h>
#include <string.h>
static int file_read(void *context, uint32_t offset, uint32_t size, void *buffer)
{
	return fseek(context, (long)offset, SEEK_SET) == 0 && fread(buffer, 1, size, context) == size;
}
/* arguments: the name to judge under, then a file and its content hash (hex) at a time */
int main(int count, char **arguments)
{
	int index;
	struct cache_file_source source;
	int campaign;
	for (index = 2; index + 1 < count; index += 2)
	{
		unsigned char header[CACHE_FILE_HEADER_BYTES] = {0};
		unsigned char content[MAP_SHARE_POLICY_CONTENT_BYTES];
		FILE *stream = fopen(arguments[index], "rb");
		long size;
		size_t bytes;
		int position;
		if (!stream)
			return 2;
		bytes = fread(header, 1, sizeof(header), stream);
		fseek(stream, 0, SEEK_END);
		size = ftell(stream);
		fclose(stream);
		for (position = 0; position < MAP_SHARE_POLICY_CONTENT_BYTES; position++)
		{
			unsigned value;
			sscanf(arguments[index + 1] + position * 2, "%2x", &value);
			content[position] = (unsigned char)value;
		}
		stream = fopen(arguments[index], "rb");
		source.context = stream;
		source.read = file_read;
		source.size = (uint32_t)size;
		campaign = map_share_policy_campaign(&source);
		fclose(stream);
		printf("%d\n", map_share_policy_may_send(map_share_policy_judge(0, arguments[1], (unsigned)strlen(arguments[1]),
			header, (unsigned)bytes, (unsigned)size, content, campaign)));
	}
	return 0;
}
''')
    judge = tmp_path / "judge"
    subprocess.run([clang, "-std=c99", "-I", str(GAME), str(driver), str(GAME / "map_share_policy.c"),
                    str(GAME / "cache_file_formats.c"), "-o", str(judge)], check=True)
    refused = CUSTOM_EDITION_STOCK | {"bitmaps", "sounds", "loc"}
    files = sorted(path for path in real_maps.glob("*.map") if path.stem in refused)
    assert files
    arguments = []
    shared = []
    conversions = 0
    for path in files:
        data = path.read_bytes()
        content = content_hash(data)
        if path.stem in CUSTOM_EDITION_STOCK:
            assert content == CUSTOM_EDITION_CONTENT[path.stem], f"{path.name}: content hash {content}"
            # (a copy whose header checksum was edited: the same content)
            edited = tmp_path / f"edited-{path.name}"
            edited.write_bytes(data[:0x64] + bytes(4) + data[0x68:])
            arguments += [str(edited), content_hash(edited.read_bytes())]
            # (a conversion of a campaign level made from it: its scenario renamed into a campaign level's folder,
            # so its content is no stock map's, and its header made a singleplayer map's; left a multiplayer map,
            # the same copy is sent)
            scenario = f"levels\\test\\{path.stem}\\{path.stem}\0".encode()
            if path.stem != "ui" and scenario in data:
                renamed = data.replace(scenario, b"levels\\a10\\" + b"x" * (len(scenario) - 12) + b"\0")
                multiplayer = tmp_path / f"multiplayer-{path.name}"
                multiplayer.write_bytes(renamed)
                shared += [str(multiplayer), content_hash(renamed)]
                converted = tmp_path / f"converted-{path.name}"
                converted.write_bytes(renamed[:0x60] + bytes(2) + renamed[0x62:])
                arguments += [str(converted), content_hash(converted.read_bytes())]
                conversions += 1
        arguments += [str(path), content]
    assert conversions, "no stock map's scenario could be renamed into a campaign level's folder"
    for name in ("hugeass", *(path.stem for path in files)):
        result = subprocess.run([str(judge), name, *arguments], capture_output=True, text=True, check=True)
        assert set(result.stdout.split()) == {"0"}, f"a stock or resource map is sent as {name}.map"
    result = subprocess.run([str(judge), "mymap", *shared], capture_output=True, text=True, check=True)
    assert result.stdout.split() == ["1"] * conversions, "a multiplayer map in a campaign level's folder is refused"
