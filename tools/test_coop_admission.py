"""CLASSIC admission, compatible closed listings, and the in-game refusal dialog."""
from pathlib import Path
import subprocess

from harness import function

ROOT = Path(__file__).resolve().parents[1]


def test_classic_admission(tmp_path):
    manager = (ROOT / 'source/networking/network_server_manager.c').read_text()
    client = (ROOT / 'source/networking/network_client_manager.c').read_text()
    production = '\n'.join(function(manager, name) for name in (
        'network_game_server_classic_fireteam',
        'network_game_server_refuse_late_joiner',
        'network_game_server_refuse_classic_late_joiner',
        'network_game_server_accepts_late_joins',
        'network_game_server_queue_player_for_addition',
        'network_game_server_queue_held_players',
    ))
    production += function(client, 'network_game_client_rejected_by_game')
    harness = r'''
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
typedef int boolean;
typedef unsigned short word;
#define MAXIMUM_NETWORK_MACHINE_COUNT 4
#define MAXIMUM_NETWORK_PLAYER_COUNT 4
#define NONE -1
#define NUMBEROF(a) (sizeof(a)/sizeof((a)[0]))
#define VALID_INDEX(n,maximum) ((n)>=0 && (n)<(maximum))
#define TEST_FLAG(flags,bit) ((flags)&(1<<(bit)))
#define csmemcpy memcpy
#define TRUE 1
#define FALSE 0
#define match_assert(...) ((void)0)
#define network_event(...) ((void)0)
enum { _network_game_server_state_pregame, _network_game_server_state_ingame,
    _network_game_server_state_postgame, _network_game_client_state_searching };
enum { _rejection_code_version_too_old, _rejection_code_version_too_new,
    _rejection_code_bad_join_token, _rejection_code_bad_password,
    _rejection_code_game_is_full, _rejection_code_game_is_closed,
    _rejection_code_blacklisted_machine, HALO_NETWORK_REJECTION_LINK_REQUIRED=8,
    HALO_NETWORK_REJECTION_VPN_BLOCKED, HALO_NETWORK_REJECTION_LINK_INVALID };
enum { _message_server_machine_rejected, _error_network_join_game_closed,
    _error_network_failed_to_join_game };
enum { _network_client_machine_level_loaded_bit, _network_client_machine_started_late_bit };
struct network_player { int machine_index, controller_index, valid, player_list_index; };
struct network_game_server_client_machine { int machine_index, joined, flags; };
struct network_game { struct { int game_engine_index; } variant; struct network_player players[4]; };
struct network_game_server { int state; struct network_game game;
    struct network_game_server_client_machine client_machines[4];
    int queued_player_valid, waiting_player_count;
    struct network_player queued_player, waiting_players[4];
};
static int network_player_is_valid(struct network_player *p) { return p->valid; }
static int network_game_server_held_player_count;
static struct network_player network_game_server_held_players[4];
struct message_server_machine_rejected { word reason; };
struct network_message { word reason; };
struct network_game_client { int state; };
struct transport_address { int unused; };
static int drop_in, open_game, free_slot, sent, dropped, aborted, reset;
static int standard_error;
static wchar_t dialog[512];
static struct network_message packet;
static int config_boolean(char const *key) { assert(!strcmp(key,"network.coop_late_join")); return drop_in; }
static int network_game_server_game_is_open(void *s) { return open_game; }
static int network_game_has_free_player_slot(void *g) { return free_slot; }
static int network_game_server_client_machine_is_joined_to_game(void *s, struct network_game_server_client_machine *m) { return m->joined; }
static struct network_message *create_network_game_message(int type, struct message_server_machine_rejected *m, size_t size) {
    assert(type==_message_server_machine_rejected && size==sizeof(*m)); packet.reason=m->reason; return &packet;
}
static void network_game_server_send_message_to_client_machine(void *s, void *m, struct network_message *p) { assert(p==&packet); sent++; }
static void network_game_server_drop_client_machine(void *s, void *m) { dropped++; }
static char const *halo_network_rejection_message(word code) { return NULL; }
static void platform_show_message(char const *title, char const *message) { assert(0); }
static void display_error_text_when_main_menu_loaded(wchar_t const *text) { wcscpy(dialog,text); }
static void display_error_when_main_menu_loaded(int code) { standard_error=code; }
static void network_game_abort(void) { aborted++; }
static void network_game_client_reset(void *c, int all) { assert(all); reset++; }
'''
    checks = r'''
int main(void) {
    struct network_game_server server={0};
    struct network_game_server_client_machine machine={.machine_index=3};
    struct network_game_client client={0};
    struct transport_address address={0};
    for (int mode=0; mode<2; mode++) for (int state=0; state<3; state++)
    for (int engine=0; engine<3; engine++) for (int joined=0; joined<2; joined++)
    for (int open=0; open<2; open++) for (int free=0; free<2; free++) {
        drop_in=mode; server.state=state; server.game.variant.game_engine_index=engine;
        machine.joined=joined; open_game=open; free_slot=free; sent=dropped=0;
        int classic=state==_network_game_server_state_ingame && engine==0 && !mode;
        assert(network_game_server_accepts_late_joins(&server)==
            (state==_network_game_server_state_ingame && !classic && open && free));
        assert(network_game_server_refuse_classic_late_joiner(&server,&machine)==(classic && !joined));
        assert(sent==(classic && !joined) && dropped==sent);
        if (sent) assert(packet.reason==5);
    }
    /* Unloaded machines cannot add controllers either. Only an original
       held lobby player may finish joining after its map download. */
    for (int mode=0; mode<2; mode++) for (int engine=0; engine<2; engine++)
    for (int state=0; state<3; state++) for (int flags=0; flags<4; flags++)
    for (int held=0; held<3; held++) {
        struct network_player player={.machine_index=1,.controller_index=2,.valid=1};
        memset(&server,0,sizeof(server));
        server.state=state; server.game.variant.game_engine_index=engine;
        server.client_machines[1].flags=flags; drop_in=mode;
        network_game_server_held_player_count=held!=0;
        network_game_server_held_players[0]=player;
        if (held==2) network_game_server_held_players[0].controller_index++;
        network_game_server_queue_player_for_addition(&server,&player);
        int blocked=!mode && !engine && state==_network_game_server_state_ingame && held!=1;
        assert(server.queued_player_valid==!blocked && server.waiting_player_count==0);
    }
    /* Release preserves the held identity through the CLASSIC check, then
       consumes it so that another controller cannot reuse the exemption. */
    memset(&server,0,sizeof(server));
    server.state=_network_game_server_state_ingame; drop_in=0;
    network_game_server_held_player_count=1;
    network_game_server_held_players[0]=(struct network_player){.machine_index=1,.controller_index=2,.valid=1};
    assert(network_game_server_queue_held_players(&server,1));
    assert(server.queued_player_valid && server.queued_player.controller_index==2);
    assert(!network_game_server_held_player_count);
    server.queued_player_valid=0;
    struct network_player extra={.machine_index=1,.controller_index=3,.valid=1};
    network_game_server_queue_player_for_addition(&server,&extra);
    assert(!server.queued_player_valid);
    standard_error=-1;
    network_game_client_rejected_by_game(&client,&address,5);
    assert(wcsstr(dialog,L"The host is not accepting late joiners."));
    assert(!wcsstr(dialog,L"CLASSIC") && !wcsstr(dialog,L"full"));
    assert(standard_error==-1 && aborted==1 && reset==1);
    dialog[0]=0;
    network_game_client_rejected_by_game(&client,&address,4);
    assert(!dialog[0] && standard_error==_error_network_join_game_closed);
    return 0;
}
'''
    source, binary = tmp_path / 'admission.c', tmp_path / 'admission'
    source.write_text(harness + production + checks)
    subprocess.run(['clang', '-std=gnu99', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_listing_and_join_request_use_the_admission_policy():
    manager = (ROOT / 'source/networking/network_server_manager.c').read_text()
    handler = (ROOT / 'source/networking/network_server_message_handler.c').read_text()
    join = function(handler, 'network_game_server_handle_message_client_join_game_request')
    assert join.index('network_game_server_refuse_classic_late_joiner(') < join.index('decode_network_game_message(')
    assert 'network_game_server_accepts_late_joins(server)' in function(manager, 'network_game_server_list')
    advertisement = function(handler, 'network_game_server_handle_message_client_broadcast_game_search')
    assert 'network_game_server_accepts_late_joins(server)' in advertisement


def test_closed_advertisement_probe_preserves_normal_join_guards(tmp_path):
    client = (ROOT / 'source/networking/network_client_manager.c').read_text()
    production = '\n'.join(function(client, name) for name in (
        'network_game_client_join_available_game',
        'network_game_client_join_first_available_game',
        'network_game_client_test_join_closed_game',
    ))
    harness = r'''
#include <string.h>
#include <assert.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define MAXIMUM_NETWORK_ADVERTISED_GAMES 1
#define _network_game_client_state_searching 1
#define NETWORK_GAME_SERVER_PORT 2302
typedef char XNADDR;
typedef char XNKEY;
typedef char XNKID;
struct transport_address {union {unsigned long long_words[4];} address; int port;};
struct network_join_parameters {char join_token[16];};
struct network_advertised_game {int platform,open;char xnaddr[32],key[32],key_id[32];};
struct network_game_client {int state,join_in_progress;void *connection;struct network_advertised_game available_games[1];};
static struct network_game_client client;
static int initiated, compatible=1,network_game_client_incompatibility_told;
static struct network_game_client *global_network_game_client_get(void){return &client;}
static int network_connection_connected(void *p){return 0;}
static int network_game_client_advertised_game_is_valid(struct network_advertised_game *p){return 1;}
static int network_game_get_local_platform(void){return 5;}
static int network_game_client_advertised_game_compatible(void *a,void *b,int c){return compatible;}
#define csmemset memset
static void transport_client_start(XNADDR const *a,XNKEY const *b,XNKID const *c,int p,struct transport_address *out){out->address.long_words[0]=1;out->port=p;}
static void network_game_generate_join_game_token(char *p){}
static int network_game_client_initiate_join_game(void *a,void *b,void *c,void *d){initiated++;return 1;}
'''
    checks = r'''
int main(void){
 client.state=1;client.connection=&client;client.available_games[0].platform=5;
 assert(!network_game_client_join_first_available_game());assert(initiated==0);
 assert(network_game_client_test_join_closed_game());assert(initiated==1);
 client.available_games[0].open=1;assert(network_game_client_join_first_available_game());assert(initiated==2);
 compatible=0;assert(!network_game_client_test_join_closed_game());assert(initiated==2);
 compatible=1;client.join_in_progress=1;assert(!network_game_client_test_join_closed_game());assert(initiated==2);
 return 0;}
'''
    source, binary = tmp_path / 'closed_join.c', tmp_path / 'closed_join'
    source.write_text(harness + production + checks)
    subprocess.run(['clang', '-std=c11', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_world_marker_logging_is_bound_to_the_network_test():
    source = (ROOT / 'port/linux/game/network_test.c').read_text()
    tick = function(source, 'network_test_log_tick')
    assert tick.index('network_test.mode == _network_test_off') < tick.index('network_test_log_devices();')
    marker = function(source, 'network_test_log_devices')
    assert 'device_get_position(object_index)' in marker
    assert 'device_get_power(object_index)' in marker
    assert 'device_set_' not in marker
