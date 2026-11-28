/* tools/test_direct_ip.py inserts the real menu handlers at PRODUCTION. */
#include "halo_direct_ip.h"
#include <assert.h>
#include <stdio.h>
#include <wchar.h>

typedef int boolean;
typedef unsigned short word;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define BUTTON_A 0
#define TEXT_FIELD_LENGTH 128
#define PASSWORD_LENGTH 32
#define MAXIMUM_ADVERTISED_GAMES 9
#define LOBBY_NAME "lobby"
struct widget_instance
{
	char *name;
	short type;
	struct widget_instance *parent, *child, *next, *focused_child;
	struct { struct { short selected_index; } list; } parameters;
};
struct event_record { short controller_index; };
struct advertised_game { unsigned char xnaddr[12]; int valid; };
static struct advertised_game games[MAXIMUM_ADVERTISED_GAMES];
static struct { void *row; } text_field;
static unsigned long now;
static int client, failures, messages, posts, joins, clipboard_joins, probe_enabled, seen_controller;
static int browse_ok = 1, join_ok = 1;
static struct halo_direct_target sent_target;
unsigned long platform_real_milliseconds(void) { return now; }
void dispose_global_network_game_client(void) { client = 0; memset(games, 0, sizeof(games)); }
void dispose_global_network_game_server(void) { }
static void *global_network_game_client_get(void) { return client ? games : NULL; }
static struct advertised_game *network_game_client_get_available_games(void *c) { assert(c == games); return games; }
static int network_game_client_advertised_game_is_valid(struct advertised_game *g) { return g->valid; }
void halo_direct_ip_probe_set(const struct halo_direct_target *target)
{
	probe_enabled = target != NULL;
	if (target) sent_target = *target;
}
static void display_error_text_deferred(const wchar_t *message, int controller)
{ assert(message && controller == NONE); messages++; }
static int campaign_fail(void) { failures++; return 0; }
static int multiplayer_player(short controller) { seen_controller = controller; return 1; }
static int ui_widget_port_browse(struct widget_instance *w, struct event_record *e, boolean *deleted)
{ (void)w; (void)e; assert(!*deleted); client = browse_ok; return browse_ok; }
boolean ui_widget_port_join_direct(struct widget_instance *w, void *g, const char *lobby,
	boolean *deleted, word port)
{
	(void)w;
	assert(g == &games[1] && !strcmp(lobby, "lobby"));
	assert(port == 2500 && seen_controller == 2);
	joins++;
	*deleted = join_ok;
	return join_ok;
}
static void text_field_end(int keep) { assert(keep); text_field.row = NULL; }
static struct widget_instance *named(struct widget_instance *w, const char *name, int n)
{ (void)w; (void)name; (void)n; return NULL; }
static int text_field_editing(struct widget_instance *w) { (void)w; return 0; }
static void text_field_show(struct widget_instance *w, const char *text, int editing)
{ (void)w; (void)text; (void)editing; }
static void ui_widget_port_post_button(short controller, int button)
{ assert(controller == 2 && button == BUTTON_A); posts++; }
static int platform_clipboard_get(char *text, int size) { snprintf(text, size, "  invite \n"); return 1; }
static int p2p_join_invite(const char *text) { assert(!strcmp(text, "invite")); clipboard_joins++; return 1; }
void platform_log(const char *format, ...);

/* PRODUCTION */

int main(void)
{
	struct widget_instance list = { .type = 3 };
	struct widget_instance join = { .name = "direct_ip_button_join", .parent = &list };
	struct widget_instance field = { .name = "op_server_address", .next = &join, .parent = &list };
	list.child = &field; list.focused_child = &field;
	struct event_record event = { 2 };
	boolean deleted = FALSE;
	assert(direct_ip_connect(&join, &event, 2, &deleted) && clipboard_joins == 1);
	strcpy(direct_address, "192.0.2.999");
	assert(!direct_ip_connect(&join, &event, 2, &deleted));
	assert(failures == 1 && messages == 1 && !probe_enabled && clipboard_joins == 1);
	strcpy(direct_address, "192.0.2.7:2500");
	assert(direct_ip_connect(&join, &event, 2, &deleted));
	assert(direct_join.pending && probe_enabled && sent_target.port == 2500);
	games[0].valid = 1;
	memcpy(games[0].xnaddr + 8, (unsigned char[]){192, 0, 2, 8}, 4);
	direct_ip_update(&list);
	assert(!posts && !joins);
	games[1].valid = 1;
	memcpy(games[1].xnaddr + 8, (unsigned char[]){192, 0, 2, 7}, 4);
	direct_ip_update(&list);
	assert(posts == 1 && !joins && list.focused_child == &join && list.parameters.list.selected_index == 1);
	assert(direct_ip_connect(&join, &event, 2, &deleted));
	assert(joins == 1 && deleted && !direct_join.pending && !probe_enabled);
	deleted = FALSE;
	assert(direct_ip_connect(&join, &event, 2, &deleted));
	assert(!games[1].valid); /* New searches cannot use a cached game. */
	now = HALO_DIRECT_IP_TIMEOUT;
	direct_ip_update(&list);
	assert(!direct_join.pending && !probe_enabled && failures == 2 && messages == 2 && joins == 1);
	/* Expiry also works across the 32-bit clock wrap. */
	now = UINT32_MAX - 100;
	assert(direct_ip_connect(&join, &event, 2, &deleted));
	now = (uint32_t)(now + HALO_DIRECT_IP_TIMEOUT);
	direct_ip_update(&list);
	assert(!direct_join.pending && failures == 3);
	browse_ok = 0;
	assert(!direct_ip_connect(&join, &event, 2, &deleted) && failures == 4 && !probe_enabled);
	browse_ok = 1;
	assert(direct_ip_connect(&join, &event, 2, &deleted));
	games[1].valid = 1;
	memcpy(games[1].xnaddr + 8, (unsigned char[]){192, 0, 2, 7}, 4);
	direct_ip_update(&list);
	join_ok = 0;
	assert(!direct_ip_connect(&join, &event, 2, &deleted) && failures == 5 && !probe_enabled);
	puts("PASS: direct IP discovery, host matching, join port, fallback, and failures");
	return 0;
}
