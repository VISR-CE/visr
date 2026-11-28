"""Run Direct IP's production parser without game data or a network host."""

from pathlib import Path
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]


def test_direct_ip_target(tmp_path):
    compiler = shutil.which("clang") or shutil.which("cc")
    if compiler is None:
        pytest.skip("Direct IP probe requires a C compiler")
    binary = tmp_path / "direct-ip-probe"
    subprocess.run([
        compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
        "--include-directory=" + str(ROOT / "port/linux/src"),
        str(ROOT / "port/ios/tests/direct_ip_probe.c"), "--output=" + str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)


def test_direct_ip_discovery_and_join(tmp_path):
    """Catch ignored typed fields, wrong-host joins, missing expiry, and lost ports."""
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    # Execute the production handlers against the network and widget boundaries.
    # The fixture asserts which host, port, and controller reach them.
    from test_network_discovery import function
    state = source[source.index("static char direct_address["):source.index("static void direct_address_done(")]
    production = state + "\n" + "\n".join(function(source, signature) for signature in (
        "static void focus_child(", "static void focus_chain(", "static void direct_ip_update(",
        "static boolean direct_link_from_clipboard(",
        "static boolean direct_ip_connect(",
    ))
    fixture = (ROOT / "port/ios/tests/direct_ip_menu_probe.c").read_text()
    program = tmp_path / "direct-ip-menu.c"
    program.write_text(fixture.replace("/* PRODUCTION */", production))
    binary = tmp_path / "direct-ip-menu"
    subprocess.run([
        "clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
        "--include-directory=" + str(ROOT / "port/linux/src"), str(program),
        "--output=" + str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)


def test_direct_ip_unicast_probe(tmp_path):
    """The socket layer must send the typed port and stop after expiry or cancel."""
    source = (ROOT / "port/linux/src/xnet.c").read_text()
    state = source[source.index("static pthread_mutex_t direct_probe_lock"):source.index("/* a dotted quad")]
    start = source.index("\t\tif (direct_ip_probe_get(&direct))")
    send = source[start:source.index("\t\t/* (the broadcast's own error", start)]
    program = tmp_path / "direct-ip-socket.c"
    program.write_text(r'''
#include "halo_direct_ip.h"
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
static unsigned long now;
static unsigned long platform_real_milliseconds(void) { return now; }
''' + state + r'''
static int sends;
static struct sockaddr_in destination;
static int posix_socket_sendto(int socket, const void *buffer, int length, int flags,
    const struct sockaddr_in *target, unsigned size)
{
    assert(socket == 3 && buffer && length == 10 && flags == 0 && size == sizeof(*target));
    destination = *target;
    sends++;
    return length;
}
#define halo_ws_htons htons
static int send_probe(void)
{
    struct sockaddr_in original = { .sin_family = AF_INET, .sin_port = htons(5150) };
    const struct sockaddr_in *address = &original;
    struct sockaddr_in target;
    struct halo_direct_target direct;
    int socket = 3, length = 10, flags = 0, result = -1;
    const void *buffer = "0123456789";
''' + send + r'''
    return result;
}
int main(void)
{
    struct halo_direct_target target;
    assert(halo_direct_ip_parse("192.0.2.7", "2500", &target));
    assert(send_probe() == -1 && sends == 0);
    halo_direct_ip_probe_set(&target);
    assert(send_probe() == 10 && sends == 1);
    assert(destination.sin_family == AF_INET && destination.sin_port == htons(2500));
    assert(!memcmp(&destination.sin_addr.s_addr, (unsigned char[]){192, 0, 2, 7}, 4));
    now = HALO_DIRECT_IP_TIMEOUT - 1;
    assert(send_probe() == 10 && sends == 2);
    now++;
    assert(send_probe() == -1 && sends == 2);
    halo_direct_ip_probe_set(&target);
    halo_direct_ip_probe_set(NULL);
    assert(send_probe() == -1 && sends == 2);
    now = UINT32_MAX - 100;
    halo_direct_ip_probe_set(&target);
    now = (uint32_t)(now + HALO_DIRECT_IP_TIMEOUT);
    assert(send_probe() == -1 && sends == 2);
    return 0;
}
''')
    binary = tmp_path / "direct-ip-socket"
    subprocess.run([
        "clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread",
        "--include-directory=" + str(ROOT / "port/linux/src"), str(program),
        "--output=" + str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
