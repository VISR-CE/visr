"""A join request while a join is under way is refused before the assertion."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANAGER = (ROOT / "source/networking/network_client_manager.c").read_text()
UI = (ROOT / "source/interface/ui_widget_event_handler_functions.c").read_text()


def test_probe_of_the_guard_verdicts():
    with tempfile.TemporaryDirectory() as directory:
        executable = Path(directory) / "network_join_guard_probe"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-o", str(executable),
                        str(ROOT / "port/ios/tests/network_join_guard_probe.c")], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr


def test_the_assertion_names_the_failing_clause():
    start = MANAGER.index("boolean network_game_client_initiate_join_game(")
    body = MANAGER[start:MANAGER.index("client->join_in_progress = TRUE;", start)]
    assert "halo_network_join_refusal_name(" in body
    assert "match_vassert(" in body and "match_assert(" not in body


def test_every_ui_call_site_asks_the_guard_first():
    # the browser's list handler and ui_widget_port_join_at_port ask before starting a join;
    # network_game_client_join_available_game has the same clauses in its own precheck
    calls = list(re.finditer(r"(?<!boolean )network_game_client_initiate_join_game\(", UI))
    assert len(calls) == 2
    for call in calls:
        assert "network_game_client_can_initiate_join(" in UI[max(0, call.start() - 2500):call.start()]
    # a refused request is ignored: it must not abort the join under way
    refused = UI.index("network_game_client_can_initiate_join(", UI.index("ui_widget_port_join_at_port("))
    assert UI.index("return FALSE;", refused) < UI.index("network_game_abort();", refused)


def test_second_join_is_refused_until_the_client_is_reset(tmp_path):
    # the production guard, extracted, against a stub client: join, join again, back out, join
    start = MANAGER.index("/* port: which clause of network_game_client_initiate_join_game")
    guard = MANAGER[start:MANAGER.index("boolean network_game_client_initiate_join_game(", start)]
    (tmp_path / "halo_network_join_guard.h").write_text(
        (ROOT / "port/linux/include/halo_network_join_guard.h").read_text())
    program = tmp_path / "sequence.c"
    program.write_text("""
#include <stdio.h>
#define NULL_ ((void*)0)
typedef int boolean;
#define TRUE 1
#define FALSE 0
#include "halo_network_join_guard.h"
enum { _network_game_client_state_searching, _network_game_client_state_joining };
struct network_connection { int up; };
struct network_game_client { int state; struct network_connection *connection; };
struct network_advertised_game { int platform; };
struct network_join_parameters { int x; };
static int events;
static void network_event(char const *f, ...) { (void)f; events++; }
static int network_connection_connected(struct network_connection *c) { return c->up; }
static int network_game_get_local_platform(void) { return 1; }
""" + guard + """
int main(void)
{
	struct network_connection connection = {0};
	struct network_game_client client = { _network_game_client_state_searching, &connection };
	struct network_advertised_game game = { 1 };
	int starts = 0;

	if (!network_game_client_can_initiate_join(&client, &game)) return 1;
	client.state = _network_game_client_state_joining; starts++;   /* the first join began */
	if (network_game_client_can_initiate_join(&client, &game)) return 2; /* the second is refused */
	if (events != 1 || starts != 1) return 3;
	client.state = _network_game_client_state_searching;           /* backed out: client reset */
	if (!network_game_client_can_initiate_join(&client, &game)) return 4;
	connection.up = 1;
	if (network_game_client_can_initiate_join(&client, &game)) return 5;
	return 0;
}
""")
    executable = tmp_path / "sequence"
    subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-I", str(tmp_path),
                    "-o", str(executable), str(program)], check=True)
    assert subprocess.run([str(executable)]).returncode == 0


def test_backing_out_of_a_join_under_way_resets_the_client():
    start = UI.index("static boolean netgame_unjoin_player(")
    body = UI[start:UI.index("\nboolean ui_widget_port_unjoin_player(", start)]
    # the branch tests the client's state (the return value), not the state's data (the timeout percent)
    assert "short client_state = network_game_client_get_state(client, &state);" in body
    assert "else if (client_state == 1)" in body and "state == 1" not in body.replace("client_state == 1", "")
    joining = body[body.index("else if (client_state == 1)"):]
    assert "network_game_client_reset(client, TRUE);" in joining[:600]
