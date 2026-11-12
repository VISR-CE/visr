"""Join request padding and refusal messages for network versions 40 and 41."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_probe_of_join_padding_and_rejection_messages():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / "network_join_wire_probe"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-o", str(executable),
                        str(ROOT / "port/ios/tests/network_join_wire_probe.c")], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


def test_probe_of_a_captured_43_join_request():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / "network_join_capture_probe"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-o", str(executable),
                        str(ROOT / "port/ios/tests/network_join_capture_probe.c")], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


def test_host_refuses_a_mismatched_join_before_decoding_it():
    handler = (ROOT / "source/networking/network_server_message_handler.c").read_text()
    start = handler.rindex("static boolean network_game_server_handle_message_client_join_game_request(\n\tstruct")
    body = handler[start:handler.index("\nstatic ", start + 1)]
    gate = body.index("halo_network_version_has_join_version_field(network_game_host_version())")
    verdict = body.index("halo_network_join_version_verdict(host_version, client_version)", gate)
    refuse = body.index("return FALSE;", verdict)
    decode = body.index("decode_network_game_message(", gate)
    assert gate < verdict < refuse < decode
    assert "verdict == HALO_NETWORK_JOIN_VERSION_TOO_OLD ? _rejection_code_version_too_old : " \
        "_rejection_code_version_too_new" in body[verdict:refuse]
    assert "_message_server_machine_rejected" in body[verdict:refuse]
