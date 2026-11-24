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


def test_backing_out_of_a_join_under_way_resets_the_client(tmp_path):
    # the production netgame_unjoin_player against stubs: while the client is joining, whatever the
    # state's data (the timeout percent) says, the join is ended by resetting the client, once
    start = UI.rindex("static boolean netgame_unjoin_player(\n")  # the definition, after the forward declaration
    production = UI[start:UI.index("\n}\n", start) + 3]
    program = tmp_path / "unjoin.c"
    program.write_text(r"""
#include <stdio.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define match_vassert(file, line, expr, text) do { if (!(expr)) return 99; } while (0)
struct widget_instance { int x; };
struct event_record { short controller_index; };
struct network_player { short machine_index, controller_index; };
struct network_game { struct network_player players[4]; };
static int client_state, state_data, resets, reset_teardown, connections_set, disposes, removals, errors;
static int client_object;
static void *global_network_game_client_get(void) { return &client_object; }
static int have_server;
static void *global_network_game_server_get(void) { return have_server ? &client_object : NULL; }
static short network_game_client_get_state(void *c, short *data) { (void)c; *data = (short)state_data; return (short)client_state; }
static void network_game_client_reset(void *c, boolean teardown) { (void)c; resets++; reset_teardown = teardown; }
static void game_connection_set(int v) { if (v == 0) connections_set++; }
static void dispose_global_network_game_client(void) { disposes++; }
static void dispose_global_network_game_server(void) { disposes++; }
static void error(int level, char const *text) { (void)level; (void)text; errors++; }
static struct network_game *network_game_client_get_game(void *c) { (void)c; static struct network_game g; return &g; }
static short network_game_client_get_local_machine_index(void) { return NONE; }
static int network_player_is_valid(struct network_player *p) { (void)p; return 0; }
static boolean network_game_client_request_remove_player(void *c, void *p) { (void)c; (void)p; removals++; return TRUE; }
static void player_ui_clear_multiplayer_autojoin_for_local_player(short c) { (void)c; }
static void player_ui_autojoin_players_to_next_multiplayer_game(void) { }
static void network_game_server_pause_countdown(void *s, boolean b) { (void)s; (void)b; }
static boolean network_game_should_accept_remote_connections(void) { return FALSE; }
""" + production + r"""
int main(void)
{
	struct widget_instance widget = {0};
	struct event_record event = {0};
	boolean deleted = FALSE;
	short data;

	/* joining (state 1): reset once with the connection torn down, whatever the timeout percent */
	for (data = 0; data <= 100; data += (data < 2 ? 1 : 49))
	{
		resets = connections_set = disposes = removals = errors = 0;
		client_state = 1; state_data = data;
		netgame_unjoin_player(&widget, &event, &deleted);
		if (resets != 1 || !reset_teardown || connections_set != 1 || disposes || removals) return 1;
	}
	/* a hosting machine's own client keeps the connection mark */
	resets = connections_set = disposes = removals = 0; client_state = 1; state_data = 1; have_server = 1;
	netgame_unjoin_player(&widget, &event, &deleted);
	if (resets != 1 || connections_set != 0) return 3;
	have_server = 0;
	/* searching (state 0): nothing to end */
	resets = connections_set = errors = 0; client_state = 0; state_data = 1;
	netgame_unjoin_player(&widget, &event, &deleted);
	if (resets || connections_set) return 2;
	return 0;
}
""")
    executable = tmp_path / "unjoin"
    subprocess.run(["clang", "-std=c11", "-Wall", "-Werror", "-Wno-unused-function", "-o", str(executable), str(program)], check=True)
    assert subprocess.run([str(executable)]).returncode == 0


def test_reset_tears_down_a_join_under_way():
    # a join under way has an open, unconnected connection: the back-out's reset must still
    # tear it down, or the next join connects over a half-open one
    start = MANAGER.index("void network_game_client_reset(")
    body = MANAGER[start:MANAGER.index("\n}\n", start)]
    assert "client->state == _network_game_client_state_joining" in body
    state_set = body.index("client->state = _network_game_client_state_searching;")
    assert body.index("join_under_way") < state_set < body.index("network_connection_disconnect(")
