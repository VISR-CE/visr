"""Exercise browser capacity warnings against the production selection paths.

Removing either warning gate, replay identity check, or one-shot clearing must
fail this probe before a password screen or network join can be reached.
"""
from pathlib import Path
import subprocess
import tempfile
from test_network_discovery import function

ROOT = Path(__file__).resolve().parents[1]


def test_join_warning(tmp_path):
    source = (ROOT / 'port/linux/game/menu_functions.c').read_text()
    state = function(source, 'static struct\n{\n\tboolean active, opening, returning;') + ' browser_join_big;'
    signatures = (
        'static long player_capacity(void)',
        'static boolean player_capacity_exceeded(long players)',
        'static boolean player_capacity_marked(long players, long maximum_players)',
        'static boolean browser_join_big_ask(',
        'static boolean browser_initialize(',
        'static void lobby_browser_end(void)\n{',
        'static boolean lobby_browser_select(struct widget_instance *widget, short controller, short row,\n\tboolean *widget_deleted, boolean confirmed)\n{',
        'static boolean browser_join(struct widget_instance *widget, short controller, boolean *widget_deleted)\n{',
        'static boolean browser_pick(',
        'static boolean browser_select(struct widget_instance *widget, struct event_record *event, short controller,\n\tboolean *widget_deleted)\n{',
        'static boolean browser_join_big_finish(',
    )
    production = '\n'.join(function(source, signature) for signature in signatures)
    harness = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "p2p.h"
typedef int boolean;
typedef unsigned char byte;
#define TRUE 1
#define FALSE 0
#define NONE -1
#define NUMBEROF(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define LOBBY_BROWSER_GAMES 4
#define LOBBY_NAME "lobby"
#define PREVIEW_NAME "preview"
#define PASSWORD_NAME "password"
#define JOIN_BIG_NAME "warning"
#define csmemcpy memcpy
enum { _multiplayer_mode_lan, _multiplayer_mode_server_browser, _multiplayer_mode_direct_link };
struct event_record { int unused; };
struct widget_instance { char const *name; short row; struct widget_instance *child, *next; boolean disabled, visible; };
struct advertised_game { unsigned char key_id[8], xnaddr[12]; int player_count, maximum_player_count, open, live, valid; };
static struct { short mode, game_count, game_chosen; struct advertised_game *games[4]; unsigned char preview_key_id[8], preview_xnaddr[12]; } multiplayer;
static struct { short count, first, chosen, controller; boolean ready, joining; unsigned long begin_time; struct p2p_listing games[4], password_game; } lobby_browser;
static long capacity = 8;
static int joins, passwords, warnings, previews, failures, backs, controller_seen, begins, stops;
static struct p2p_listing joined, available[4];
static int available_count;
static struct widget_instance join_button = { .name="join_game_button_join", .row=NONE };
/* STATE */
static boolean browser_initialize(struct widget_instance *, struct event_record *, boolean *);
static void lobby_browser_end(void);
static void lobby_browser_begin(void) { begins++; lobby_browser.first=lobby_browser.chosen=lobby_browser.count=0; }
void p2p_lobby_browse(int active) { if (!active) stops++; }
static void server_browser_initialize(struct widget_instance *w) { (void)w; }
static void visible_set(struct widget_instance *w, boolean v) { (void)w; (void)v; }
static boolean browser_item_usable(struct widget_instance *w) { (void)w; return 1; }
static void browser_focus(struct widget_instance *w) { (void)w; }
static boolean ui_widget_port_browse(struct widget_instance *w, struct event_record *e, boolean *d) { (void)w; (void)e; (void)d; return 1; }

static void platform_log(char const *format, ...) { (void)format; }
static long config_integer(char const *key) { (void)key; return capacity; }
static int config_boolean(char const *key) { (void)key; return 1; }
static int campaign_fail(void) { failures++; return 0; }
static void *global_network_game_client_get(void) { return &multiplayer; }
static int network_game_client_advertised_game_compatible(void *c, struct advertised_game *g, int tell) { (void)c; (void)tell; return g->valid; }
static int advertised_in_progress(struct advertised_game *g) { return g->live; }
static int multiplayer_player(short c) { controller_seen=c; return 1; }
static int ui_widget_port_join(struct widget_instance *w, struct advertised_game *g, char const *name, boolean *deleted) { (void)w; (void)g; (void)name; (void)deleted; joins++; return 1; }
static int ui_widget_port_open(struct widget_instance *w, char const *name, boolean *deleted) { (void)w; *deleted=1; if (!strcmp(name, JOIN_BIG_NAME)) { warnings++; lobby_browser_end(); } else if (!strcmp(name, PASSWORD_NAME)) passwords++; else if (!strcmp(name, PREVIEW_NAME)) previews++; else assert(0); return 1; }
static void ui_widget_port_go_back(struct widget_instance *w) { boolean deleted=0; backs++; browser_initialize(w,NULL,&deleted); }
static struct widget_instance *ui_widget_port_active_screen(short p) { (void)p; return &join_button; }
static struct widget_instance *named(struct widget_instance *w, char const *name, int n) { (void)w; (void)n; return !strcmp(name,"join_game_button_join") ? &join_button : NULL; }
static short browser_row_index(struct widget_instance *w) { return w->row; }
static void browser_games_read(void) { }
int p2p_lobby_games(struct p2p_listing *out, int max) { assert(available_count<=max); memcpy(out, available, sizeof(available)); return available_count; }
static short lobby_browser_games_read(struct p2p_listing *games) { return (short)p2p_lobby_games(games, LOBBY_BROWSER_GAMES); }
static struct advertised_game *lobby_browser_joined_game(void) { return NULL; }
static int lobby_browser_join(struct p2p_listing const *g, short c) { joined=*g; controller_seen=c; joins++; return 1; }
void p2p_lobby_refresh(void) { }
static unsigned long system_milliseconds(void) { return 0; }
#define SOUND_FORWARD 0
static void ui_play_audio_feedback_sound(int s) { (void)s; }
/* PRODUCTION */
static void reset(int mode)
{
    memset(&browser_join_big, 0, sizeof(browser_join_big));
    memset(&multiplayer, 0, sizeof(multiplayer));
    memset(&lobby_browser, 0, sizeof(lobby_browser));
    joins=passwords=warnings=previews=failures=backs=begins=stops=0; capacity=8;
    multiplayer.mode=mode;
    available_count=lobby_browser.count=1;
    memset(available, 0, sizeof(available));
    available[0].identifier[0]=42; strcpy(available[0].invite, "original invite");
    available[0].open=available[0].joinable=1;
    available[0].player_count=4; available[0].maximum_player_count=16;
    lobby_browser.games[0]=available[0];
}
int main(void)
{
    struct widget_instance row={.name="server_item_1", .row=0}; boolean deleted=0;
    struct advertised_game lan={.key_id={9}, .xnaddr={7}, .player_count=4, .maximum_player_count=16, .open=1, .valid=1};
    for (int mode=0; mode<=1; mode++) for (int button=0; button<=1; button++)
    {
        reset(mode); multiplayer.game_count=1; multiplayer.games[0]=&lan;
        struct widget_instance *pick=button ? &join_button : &row;
        assert(browser_select(pick,NULL,2,&deleted));
        assert(warnings==1 && !joins && !passwords && browser_join_big.active && !stops);
        assert(browser_join_big_finish(&join_button,&deleted,TRUE));
        assert(joins==1 && controller_seen==2 && !browser_join_big.active && !begins);
        assert(!browser_join_big_finish(&join_button,&deleted,TRUE)); assert(joins==1);
        if (mode==1) assert(!strcmp(joined.invite,"original invite"));
        browser_select(pick,NULL,2,&deleted); assert(warnings==2 && joins==1);
        reset(mode); multiplayer.game_count=1; multiplayer.games[0]=&lan;
        browser_select(pick,NULL,1,&deleted);
        assert(browser_join_big_finish(&join_button,&deleted,FALSE));
        assert(!browser_join_big.active && !joins && backs==1);
        browser_select(pick,NULL,1,&deleted); assert(warnings==2);
        reset(mode); multiplayer.game_count=1; multiplayer.games[0]=&lan;
        capacity=0; browser_select(pick,NULL,1,&deleted); assert(!warnings && joins==1);
        reset(mode); multiplayer.game_count=1; multiplayer.games[0]=&lan;
        capacity=16; browser_select(pick,NULL,1,&deleted); assert(!warnings && joins==1);
        reset(mode); multiplayer.game_count=1; multiplayer.games[0]=&lan;
        browser_select(pick,NULL,1,&deleted);
        if (mode==1) available_count=0; else lan.key_id[0]++;
        assert(!browser_join_big_finish(&join_button,&deleted,TRUE)); assert(!joins && failures==1 && !browser_join_big.active);
        lan.key_id[0]=9;
    }
    reset(1); lobby_browser.games[0].locked=available[0].locked=1;
    browser_select(&row,NULL,3,&deleted); assert(warnings==1 && !passwords);
    assert(browser_join_big_finish(&join_button,&deleted,TRUE));
    assert(passwords==1 && !joins && lobby_browser.controller==3);
    assert(!strcmp(lobby_browser.password_game.invite,"original invite"));
    reset(1); browser_select(&row,NULL,1,&deleted);
    available[1]=available[0]; strcpy(available[1].invite,"changed invite"); available[0].identifier[0]=99; available_count=2;
    assert(browser_join_big_finish(&join_button,&deleted,TRUE));
    assert(joins==1 && joined.identifier[0]==42 && !strcmp(joined.invite,"original invite"));
    reset(0); lan.live=1; multiplayer.game_count=1; multiplayer.games[0]=&lan;
    browser_select(&row,NULL,2,&deleted); assert(warnings==1 && !previews);
    assert(browser_join_big_finish(&join_button,&deleted,TRUE)); assert(previews==1 && !joins);
    reset(0); multiplayer.game_count=2; multiplayer.games[0]=multiplayer.games[1]=&lan;
    row.row=1; browser_select(&row,NULL,1,&deleted);
    assert(multiplayer.game_chosen==1);
    browser_join_big_finish(&join_button,&deleted,FALSE);
    assert(multiplayer.game_chosen==1 && !joins); row.row=0;
    reset(1); browser_select(&row,NULL,1,&deleted);
    lobby_browser_end(); assert(!browser_join_big.active && stops==1);
    reset(0); multiplayer.game_count=1; multiplayer.games[0]=&lan;
    lan.live=0; lan.player_count=3; browser_select(&row,NULL,1,&deleted);
    assert(!warnings && joins==1); lan.player_count=4;
    reset(2); multiplayer.game_count=1; multiplayer.games[0]=&lan; lan.live=0;
    browser_select(&row,NULL,1,&deleted); assert(!warnings && joins==1);
    puts("join warning: row/Join, LAN/server, confirm once, cancel, capacity, password order, stale/reordered games, preview, Direct Link passed");
}
'''
    program = harness.replace('/* STATE */', state).replace('/* PRODUCTION */', production)
    probe, binary = tmp_path / 'join_warning.c', tmp_path / 'join_warning'
    probe.write_text(program)
    subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-iquote', str(ROOT / 'port/linux/src'), str(probe),
                    '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='join-warning-') as temporary:
        test_join_warning(Path(temporary))
