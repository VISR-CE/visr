"""Compile browser filtering, map lookup, and controller focus paths in isolation."""

from pathlib import Path
import subprocess
import tempfile

from test_network_discovery import function


ROOT = Path(__file__).resolve().parents[1]


def test_browser_layout_c_filters_counts_and_target(tmp_path):
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    sorting_start = source.index("static struct\n", source.index("#define BROWSER_SORT_KEY_LENGTH"))
    sorting = function(source[sorting_start:], "static struct\n") + " browser_sorting;"
    signatures = (
        "static boolean browser_listing_cooperative(",
        "static boolean browser_listing_can_join(",
        "static boolean browser_listing_other_filters(",
        "static boolean browser_listing_in_chip(",
        "static boolean browser_listing_shown(",
        "static int browser_key_compare(",
        "static int browser_order_compare(",
        "static short browser_filter_and_sort(",
        "static long browser_target_fps(",
    )
    production = "\n".join(function(source, signature) for signature in signatures)
    harness = r'''
#define HALO_IOS 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "p2p.h"
#include "halo_network_content.h"
typedef int boolean;
#define FALSE 0
#define TRUE 1
#define LOBBY_BROWSER_GAMES 16
#define BROWSER_SORT_KEY_LENGTH 48
#define ROW_TEXT_LENGTH 128
#define ustrncpy wcsncpy
enum { _browser_sort_players, _browser_sort_name, _browser_sort_map, _browser_sort_game };
static struct { short chip_counts[3]; } lobby_browser;
static int show_full=1, show_locked=1, show_unjoinable=0;
static char const *game_type="all", *frame_rate="auto";
static char const *base_rate="30";   /* the predictable/in-force value: must never be read for the target */
static int display_refresh=0;
static int host_sdl_display_target_fps(void) { return display_refresh; }
static int config_variable_string(char const *key, char *text, size_t size)
{
    assert(!strcmp(key,"display.frame_rate")); snprintf(text,size,"%s",frame_rate); return 1;
}
static int config_boolean(char const *key)
{
    if (!strcmp(key,"network.browser_show_full")) return show_full;
    if (!strcmp(key,"network.browser_show_locked")) return show_locked;
    assert(!strcmp(key,"network.browser_show_unjoinable")); return show_unjoinable;
}
static char const *config_string(char const *key)
{
    if (!strcmp(key,"display.frame_rate")) return base_rate;
    assert(!strcmp(key,"network.browser_games")); return game_type;
}
static short browser_sort(void) { return _browser_sort_players; }
static void text_to_wide(char const *text, wchar_t *out, short size)
{
    (void)text; (void)out; (void)size; assert(0);
}
static void map_display_name(char const *name, wchar_t *out)
{
    (void)name; (void)out; assert(0);
}
static void game_kind_type_text(unsigned kind, long engine, wchar_t *out, short size)
{
    (void)kind; (void)engine; (void)out; (void)size; assert(0);
}
/* SORTING */
/* PRODUCTION */
static struct p2p_listing listing(unsigned engine, unsigned players)
{
    struct p2p_listing g={0};
    g.engine_type=engine; g.player_count=players; g.maximum_player_count=16;
    g.joinable=g.open=1;
    return g;
}
static void counts(char const *type, short shown, short all, short multiplayer, short coop)
{
    struct p2p_listing games[7]; short hidden;
    games[0]=listing(0,2); games[1]=listing(2,8); games[2]=listing(1,4);
    games[3]=listing(0,1); games[3].locked=1;
    games[4]=listing(2,16);
    games[5]=listing(0,3); games[5].failed=1;
    games[6]=listing(2,7); games[6].kind=1;
    game_type=type;
    assert(browser_filter_and_sort(games,7,&hidden)==shown);
    /* only the selected chip's games count as hidden: the other chips' games are not hidden by a filter */
    assert(hidden==(!strcmp(type,"all")?7:!strcmp(type,"coop")?3:4)-shown);
    assert(lobby_browser.chip_counts[0]==all);
    assert(lobby_browser.chip_counts[1]==multiplayer);
    assert(lobby_browser.chip_counts[2]==coop);
    for (int i=0;i<shown;i++) assert(browser_listing_shown(&games[i]));
    for (int i=1;i<shown;i++)
        assert(browser_listing_can_join(&games[i-1]) || !browser_listing_can_join(&games[i]));
}
int main(void)
{
    struct p2p_listing g=listing(2,4);
    /* Defaults retain full/password games and hide every incompatible reason. */
    assert(browser_listing_other_filters(&g) && browser_listing_shown(&g));
    assert(browser_listing_can_join(&g) && !browser_listing_cooperative(&g));
    g.engine_type=0; assert(browser_listing_cooperative(&g));
    game_type="multiplayer"; assert(!browser_listing_shown(&g));
    game_type="coop"; assert(browser_listing_shown(&g));
    g.engine_type=2; assert(!browser_listing_shown(&g));
    game_type=NULL; assert(browser_listing_shown(&g));
    game_type="all";
    g.locked=1; assert(browser_listing_shown(&g));
    show_locked=0; assert(!browser_listing_other_filters(&g));
    show_locked=1; g.locked=0;
    g.player_count=16; assert(browser_listing_other_filters(&g));
    show_full=0; assert(!browser_listing_other_filters(&g));
    g.player_count=17; assert(!browser_listing_other_filters(&g));
    g.maximum_player_count=0; assert(browser_listing_other_filters(&g));
    g.open=0; assert(!browser_listing_other_filters(&g));
    show_full=1; assert(browser_listing_other_filters(&g));
    g=listing(2,4);
    g.joinable=0; assert(!browser_listing_can_join(&g) && !browser_listing_shown(&g));
    show_unjoinable=1; assert(browser_listing_shown(&g));
    show_unjoinable=0; g.joinable=1; g.failed=1;
    assert(!browser_listing_can_join(&g) && !browser_listing_shown(&g));
    g.failed=0;
    unsigned const unsupported[]={1,2,3,4,0x80,0xff};
    for (unsigned i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++)
    {
        g.kind=unsupported[i]; assert(!browser_listing_can_join(&g));
        assert(!browser_listing_shown(&g));
        show_unjoinable=1; assert(browser_listing_shown(&g)); show_unjoinable=0;
    }
    /* A chip counts its prospective results, independently of the chosen type. */
    counts("all",5,5,3,2); counts("coop",2,5,3,2); counts("multiplayer",3,5,3,2);
    show_full=show_locked=0;
    counts("all",3,3,2,1); counts("coop",1,3,2,1); counts("multiplayer",2,3,2,1);
    show_unjoinable=1;
    counts("all",5,5,3,2); counts("coop",2,5,3,2); counts("multiplayer",3,5,3,2);
    struct p2p_listing empty[1]; short hidden=-1;
    assert(browser_filter_and_sort(empty,0,&hidden)==0 && hidden==0);
    assert(!lobby_browser.chip_counts[0] && !lobby_browser.chip_counts[1] && !lobby_browser.chip_counts[2]);
    /* The target is the profile's variable-workload rate, not the in-force (predictable) one. */
    frame_rate="60"; assert(browser_target_fps()==60);
    char const *rates[]={"30","60","90","120","240","1000"};
    for (unsigned i=0;i<sizeof(rates)/sizeof(rates[0]);i++)
    {
        frame_rate=rates[i]; assert(browser_target_fps()==strtol(rates[i],NULL,10));
    }
    /* "display" and "unlimited" read the display's refresh where known, else 60. */
    char const *open[]={"display","unlimited","auto","","0","29","1001","-60","60fps"};
    for (unsigned i=0;i<sizeof(open)/sizeof(open[0]);i++)
    {
        display_refresh=0; frame_rate=open[i]; assert(browser_target_fps()==60);
        display_refresh=120; assert(browser_target_fps()==120);
        display_refresh=60; assert(browser_target_fps()==60);  /* a 60 Hz display, or 120 Hz halved by Low Power Mode */
        display_refresh=30; assert(browser_target_fps()==30);  /* 60 Hz halved by Low Power Mode */
    }
    display_refresh=0;
    puts("browser layout C: filters, type chips, incompatibility reasons, empty counts, gameplay target FPS passed");
}
'''
    program = harness.replace("/* SORTING */", sorting).replace("/* PRODUCTION */", production)
    probe, binary = tmp_path / "browser_layout_c.c", tmp_path / "browser_layout_c"
    probe.write_text(program)
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-iquote", str(ROOT / "port/linux/src"),
                    "-iquote", str(ROOT / "port/linux/include"), str(probe),
                    "--output=" + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)




def test_browser_map_picture_lookup(tmp_path):
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    production = "\n".join(function(source, signature) for signature in (
        "static char const *scenario_name(", "static short campaign_level_of(",
        "static void lobby_map_show(struct widget_instance *description, char const *map_name)\n{"))
    harness = r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#define NONE -1
#define NUMBER_OF_SINGLE_PLAYER_LEVELS 2
#define _stricmp strcasecmp
struct widget_instance {
    int visible;
    struct { short current_frame_index; } animation;
    struct { struct { short string_list_index; } text_box; } parameters;
};
static struct widget_instance mp, sp, mp_name, sp_name;
static char const *const maps[]={"levels\\test\\bloodgulch\\bloodgulch", "levels\\test\\damnation\\damnation"};
static char const *main_get_solo_level_name(short level) {
    return level ? "levels\\b30\\b30" : "levels\\a10\\a10";
}
static int custom_edition_level_name(char const *name) { return !strncmp(name,"custom_maps",11); }
static short custom_edition_maps_display_index(char const *name) {
    return !strcmp(name,"custom_maps\\example") ? 32 : NONE;
}
static short ui_widget_port_multiplayer_maps(char const *const **names, void *unused) {
    (void)unused; *names=maps; return 2;
}
static short xbox_multiplayer_map_count(short count) { return count; }
static struct widget_instance *named(struct widget_instance *parent, char const *name, int unused) {
    (void)parent; (void)unused;
    return !strcmp(name,"lobby_map_pic") ? &mp : !strcmp(name,"replay_level_right_pic") ? &sp :
        !strcmp(name,"lobby_map_name") ? &mp_name : &sp_name;
}
static void visible_set(struct widget_instance *widget,int visible) { widget->visible=visible; }
/* PRODUCTION */
int main(void) {
    char const *bloodgulch[]={"bloodgulch","BloodGulch","levels\\test\\bloodgulch\\bloodgulch",
        "levels/test/bloodgulch/bloodgulch"};
    for (unsigned i=0;i<sizeof(bloodgulch)/sizeof(*bloodgulch);i++) {
        lobby_map_show(&mp,bloodgulch[i]);
        assert(mp.visible && !sp.visible && mp.animation.current_frame_index==0);
    }
    lobby_map_show(&mp,"damnation"); assert(mp.animation.current_frame_index==1);
    lobby_map_show(&mp,"unknown"); assert(mp.animation.current_frame_index==19);
    lobby_map_show(&mp,"b30"); assert(!mp.visible && sp.visible && sp.animation.current_frame_index==1);
    lobby_map_show(&mp,"levels/a10/a10"); assert(!mp.visible && sp.visible && sp.animation.current_frame_index==0);
    lobby_map_show(&mp,"custom_maps\\example"); assert(mp.visible && !sp.visible && mp.animation.current_frame_index==32);
    lobby_map_show(&mp,"custom_maps\\a10"); assert(mp.visible && !sp.visible && mp.animation.current_frame_index==19);
    lobby_map_show(&mp,"custom_maps\\bloodgulch"); assert(mp.visible && !sp.visible && mp.animation.current_frame_index==19);
}
'''
    _compile_probe(tmp_path, "browser_map_picture", harness.replace("/* PRODUCTION */", production))


def _compile_probe(tmp_path, name, program):
    probe, binary = tmp_path / (name + ".c"), tmp_path / name
    probe.write_text(program)
    subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(probe), "--output=" + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_browser_controller_navigation(tmp_path):
    source = (ROOT / "port/linux/game/menu_functions.c").read_text()
    signatures = (
        "static short browser_row_index(",
        "static void lobby_browser_focus_row(",
        "static short lobby_browser_rows(",
        "static void browser_focus(",
        "static void browser_c_controls_update(",
        "static void browser_c_focus(struct widget_instance *widget)\n{",
        "static void browser_ready_post(",
        "static boolean browser_chips_navigation(",
        "static boolean browser_rows_navigation(",
    )
    production = "\n".join(function(source, signature) for signature in signatures)
    harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define HALO_MENU_FOCUS_ROW 0
static int ui_widget_port_draws_focus(int kind, int focused) { (void)kind; return focused; }
#define NONE -1
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define PIN(x,a,b) MIN(MAX(x,a),b)
#define NUMBEROF(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define BROWSER_ROWS 15
#define ROW_TEXT_LENGTH 256
#define BUTTON_A 7
#define EVENT_LEFT_STICK 1
#define _gamepad_binary_button_dpad_right 2
struct event_record { int type; union { struct { float x; } stick; struct { int index; } button; } data; };
struct widget_instance {
    char const *name;
    struct widget_instance *parent,*child,*next,*focused_child;
    struct { struct { short selected_index; } list; } parameters;
    boolean visible,disabled;
    struct { short current_frame_index; } animation;
};
static struct { short count,chosen,first,controller,chip_counts[3]; } lobby_browser;
static struct widget_instance *expected_post_row;
static short expected_post_controller;
static int posts;
static struct widget_instance *screen_of(struct widget_instance *w)
{
    while (w->parent) w=w->parent;
    return w;
}
static struct widget_instance *named(struct widget_instance *w, char const *name, int unused)
{
    (void)unused;
    if (!w) return NULL;
    if (!strcmp(w->name,name)) return w;
    for (struct widget_instance *child=w->child;child;child=child->next)
    {
        struct widget_instance *result=named(child,name,0);
        if (result) return result;
    }
    return NULL;
}
static void ui_widget_port_post_button(short controller, short button)
{
    struct widget_instance *row=expected_post_row, *parent=row->parent;
    assert(controller==expected_post_controller && button==BUTTON_A);
    for (struct widget_instance *w=row;w->parent;w=w->parent)
        assert(w->parent->focused_child==w);
    assert(parent->parameters.list.selected_index==atoi(row->name+12)-1);
    posts++;
}
static char const *const browser_chip_values[] = { "all", "multiplayer", "coop" };
static char const *const browser_chip_names[] = { "browser_chip_all", "browser_chip_multiplayer", "browser_chip_coop" };
static wchar_t const *const browser_filter_label = L"FILTER";
static char const *config_string(char const *key) { (void)key; return "all"; }
static void text_set(struct widget_instance *widget, wchar_t const *text) { (void)widget; (void)text; }
static void visible_set(struct widget_instance *widget, boolean visible) { if (widget) widget->visible=visible; }
static void usnprintf(wchar_t *text, int size, wchar_t const *format, ...) {
    (void)size; (void)format; *text=0;
}
static void focus_off_hidden(struct widget_instance *widget) { (void)widget; }
/* PRODUCTION */
static void children(struct widget_instance *parent,struct widget_instance *items,int count)
{
    parent->child=items;
    for (int i=0;i<count;i++)
    {
        items[i].parent=parent;
        items[i].next=i+1<count ? &items[i+1] : NULL;
    }
}
int main(void)
{
    struct widget_instance screen={.name="screen"};
    struct widget_instance sections[4]={{.name="browser_chips"},{.name="join_game_items_list"},
        {.name="browser_detail"},{.name="browser_actions"}};
    struct widget_instance chips[4]={{.name="browser_chip_all"},{.name="browser_chip_multiplayer"},
        {.name="browser_chip_coop"},{.name="browser_filter"}};
    struct widget_instance rows[6]={{.name="server_item_1"},{.name="server_item_2"},
        {.name="server_item_3"},{.name="server_item_4"},{.name="server_item_5"},{.name="server_item_6"}};
    struct widget_instance legend[3]={{.name="browser_ip_address",.visible=TRUE},{.name="browser_create_inter",.visible=TRUE},
        {.name="browser_back",.visible=TRUE}};
    children(&screen,sections,4); children(&sections[0],chips,4); children(&sections[1],rows,6);
    children(&sections[3],legend,3);
    for (int i=0;i<6;i++) rows[i].visible=TRUE;
    struct widget_instance *list=&sections[1];
    assert(lobby_browser_rows(list)==6);
    browser_c_focus(&rows[3]);
    assert(list->focused_child==&rows[3] && list->parameters.list.selected_index==3);
    assert(screen.focused_child==list && screen.parameters.list.selected_index==1);
    lobby_browser.count=9; lobby_browser.first=0; lobby_browser.chosen=3;
    browser_rows_navigation(&rows[3],TRUE);
    assert(list->focused_child==&rows[4] && lobby_browser.chosen==4);
    browser_c_focus(&rows[5]); lobby_browser.chosen=5;
    for (int first=1;first<=3;first++)
    {
        browser_rows_navigation(&rows[5],TRUE);
        assert(lobby_browser.first==first && lobby_browser.chosen==first+5);
        assert(list->focused_child==&rows[5] && screen.focused_child==list);
    }
    browser_rows_navigation(&rows[5],TRUE);
    assert(screen.focused_child==list && list->focused_child==&rows[5]);
    assert(lobby_browser.first==3 && lobby_browser.chosen==8);
    browser_chips_navigation(&legend[0]);
    assert(screen.focused_child==list && list->focused_child==&rows[5]);
    browser_c_focus(&rows[0]);
    for (int first=2;first>=0;first--)
    {
        browser_rows_navigation(&rows[0],FALSE);
        assert(lobby_browser.first==first && lobby_browser.chosen==first);
        assert(screen.focused_child==list);
    }
    browser_rows_navigation(&rows[0],FALSE);
    assert(screen.focused_child==&sections[0] && sections[0].focused_child==&chips[0]);
    browser_c_controls_update(list);
    assert(chips[0].animation.current_frame_index==3);
    assert(chips[1].animation.current_frame_index==0 && chips[2].animation.current_frame_index==0);
    browser_focus(list); /* Rendering must clear the old row's local focus. */
    assert(list->focused_child==NULL);
    /* (the chosen game's tab stays joined to the panel while the focus is elsewhere; no other row is) */
    for (int i=0;i<6;i++) assert(rows[i].animation.current_frame_index==(i+lobby_browser.first==lobby_browser.chosen && rows[i].visible));
    lobby_browser.chosen=2; browser_chips_navigation(&chips[1]);
    assert(screen.focused_child==list && list->focused_child==&rows[2]);
    browser_c_controls_update(list);
    assert(chips[0].animation.current_frame_index==1);
    browser_focus(list); assert(rows[2].animation.current_frame_index==1);
    browser_chips_navigation(&sections[2]);
    assert(screen.focused_child==list && list->focused_child==&rows[2]);
    /* Async completion and its retry must reach the selected row after focus moved away. */
    expected_post_row=&rows[2]; expected_post_controller=lobby_browser.controller=3;
    browser_c_focus(&chips[1]); browser_ready_post(list);
    assert(posts==1 && screen.focused_child==list && list->focused_child==&rows[2]);
    expected_post_controller=lobby_browser.controller=1;
    browser_c_focus(&legend[2]); browser_ready_post(list);
    assert(posts==2 && screen.focused_child==list && list->focused_child==&rows[2]);
    list->focused_child=NULL; lobby_browser.chosen=0; expected_post_row=&rows[0];
    browser_c_focus(&chips[2]); browser_ready_post(list);
    assert(posts==3 && list->focused_child==&rows[0]);
    /* With no games the chips' down stays on the chips (the actions are the engine's bar, not focus targets). */
    lobby_browser.count=0;
    browser_chips_navigation(&chips[0]);
    assert(screen.focused_child==&sections[0] && sections[0].focused_child==&chips[0]);
}
'''
    _compile_probe(tmp_path, "browser_navigation", harness.replace("/* PRODUCTION */", production))


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="browser-layout-c-") as temporary:
        path = Path(temporary)
        test_browser_layout_c_filters_counts_and_target(path)
        test_browser_map_picture_lookup(path)
        test_browser_controller_navigation(path)


def _png_alpha_column(path, x):
    import struct
    import zlib
    data = path.read_bytes()
    pos, idat, header = 8, b'', None
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat += body
        pos += 12 + length
    width, height, depth, color, _, _, interlace = header
    assert (depth, color, interlace) == (8, 6, 0), 'expected 8-bit RGBA, not interlaced'
    stride, raw = width * 4, zlib.decompress(idat)
    rows, prev = [], bytearray(stride)
    for y in range(height):
        ft, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if ft == 1:
                line[i] = (line[i] + a) & 255
            elif ft == 2:
                line[i] = (line[i] + b) & 255
            elif ft == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif ft == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 255
        rows.append(line)
        prev = line
    return [row[x * 4 + 3] for row in rows]


def test_backdrop_has_bottom_rule_above_the_legend_strip():
    root = Path(__file__).resolve().parents[1] / 'port/assets/menus/xbox'
    alpha = _png_alpha_column(root / 'art/browser/backdrop.png', 1)
    assert len(alpha) == 960                       # 480 design lines at scale 2
    assert alpha[10] == 0                          # clear under the title
    assert alpha[132] > 150                        # top rule at y=66
    assert alpha[818] > 150                        # bottom rule at y=409, above the legend
    assert 150 < alpha[830] < 255                 # dark strip 410..444 behind the legend
    assert alpha[-1] == 0                          # nothing below the strip
    text = (root / 'main_menu.multiplayer_type_select.join_game.xml').read_text()
    assert 'bitmap="xbox/browser/backdrop"' in text
    filters = (root / 'main_menu.multiplayer_type_select.join_game.filters.xml').read_text()
    assert 'bitmap="xbox/browser/filters_backdrop"' in filters


def test_every_detail_widget_the_empty_panel_hides_is_shown_or_hidden_for_a_game():
    """Bug class (the pills missing on first open): the panel blanks widgets when no game is chosen, and a game
    chosen later must set each of them itself instead of relying on what an earlier state left."""
    import re
    from pathlib import Path
    source = (Path(__file__).resolve().parents[1] / "port/linux/game/menu_functions.c").read_text()
    start = source.index("\tif (!present)\n\t{\n\t\tchar const *const blank[]")
    end = source.index("\n}\n", start)
    body = source[start:end]
    hidden = re.search(r"hidden\[\] = \{([^}]*)\}", body).group(1)
    names = re.findall(r'"(\w+)"', hidden)
    after = body[body.index("game = &lobby_browser.games"):]
    for name in names:
        assert re.search(r'visible_set\(\s*named\(detail, "%s", 0\)' % name, after), name


def test_connect_progress_belongs_to_the_joining_game_only():
    """Bug class (the panel showed another game's details with "Connecting to X..."): progress is the joining
    game's row and one line in its own panel, a failed join is a dialog, and the legend never repeats it."""
    from pathlib import Path
    source = (Path(__file__).resolve().parents[1] / "port/linux/game/menu_functions.c").read_text()
    assert source.count('L"Connecting to %s..."') == 1
    line = source.index('L"Connecting to %s..."')
    guard = source.rindex("boolean joining_chosen", 0, line)
    assert "lobby_browser.identifier" in source[guard:line]
    assert "CONNECTING..." not in source
    detail = source[source.index("static void browser_c_detail_update("):]
    assert "ui_widget_port_bar_caption" not in detail[:detail.index("\n}\n")]
    timeout = source[source.index("could not reach the server browser's game"):]
    assert "display_error_text_deferred" in timeout[:900]


def test_deferred_error_text_is_copied_not_borrowed():
    """Bug class (the failed-join dialog read a dead stack buffer): the dialog reads the pending text on every
    frame it draws, so display_error_text_deferred must own a copy, as its main-menu twin does."""
    import re
    import shutil
    import subprocess
    import tempfile
    from pathlib import Path
    root = Path(__file__).resolve().parents[1]
    source = (root / "source/interface/ui_widget.c").read_text()
    start = source.index("void display_error_text_deferred(")
    function = source[start:source.index("\n}\n", start) + 3]
    compiler = shutil.which("clang") or shutil.which("cc")
    if compiler is None:
        import pytest
        pytest.skip("A C compiler is required")
    program = r'''
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
typedef wchar_t wchar;
#define NONE (-1)
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define VALID_INDEX(i, n) ((i) >= 0 && (i) < (n))
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define _error_cannot_create_saved_game_file_with_empty_name 7
#define TRUE 1
#define FALSE 0
static struct { struct { int error_code; } deferred_errors[4]; } widget_globals = { { { NONE }, { NONE }, { NONE }, { NONE } } };
static wchar_t const *ui_widget_port_error_pending_text;
static wchar_t *ustrncpy(wchar_t *d, wchar_t const *s, size_t n) { return wcsncpy(d, s, n); }
static void display_error_deferred(int code, short player, int a, int b) { (void)code; (void)player; (void)a; (void)b; }
''' + function + r'''
int main(void)
{
	wchar_t buffer[32];
	wcscpy(buffer, L"Couldn't reach it.");
	display_error_text_deferred(buffer, NONE);
	wcscpy(buffer, L"garbage garbage garbage");
	if (wcscmp(ui_widget_port_error_pending_text, L"Couldn't reach it.")) { fprintf(stderr, "borrowed\n"); return 1; }
	return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="deferred-error-") as temporary:
        path = Path(temporary) / "probe.c"
        path.write_text(program)
        subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", str(path),
                        "--output", str(Path(temporary) / "probe")], check=True)
        subprocess.run([str(Path(temporary) / "probe")], check=True)
