/* A map load while a movie plays (the startup intro under a join link or an
init.txt map_name): game_precache_new_map ends the movie before the precache
takes the texture cache's stolen memory again and before scenario_unload
asserts that no movie plays, and it does not send the game back to the main
menu in the middle of the load. */
#include "harness.h"
#undef match_assert
#define match_assert(file, line, condition) CHECK(condition, "%s:%d", file, line)
#define match_vassert(file, line, condition, message) CHECK(condition, "%s:%d: %s", file, line, message)

#define SET_FLAG(f, b, v) ((v) ? ((f)|=(unsigned)FLAG(b)) : ((f)&=(unsigned)~FLAG(b)))
#define _error_silent 0
#define _cached_map_file_failed 2
#define BINK_FEATHER_NONE 0

/* bink_playback.h's flags, taken from the sources */
#include "bink_flags.inc"

typedef struct { long FrameNum, Frames; } *HBINK;
struct bink_playback_globals { boolean initialized; unsigned long flags; HBINK bink; long rendered_frame_count; };
static struct bink_playback_globals bink_globals;
static boolean bink_saved_frame_rate_throttle = TRUE, global_frame_rate_throttle, bink_edge_primed;
static long bink_feather_mode, bink_start_press_count;

/* the world */
static boolean stolen_memory;     /* xbox_texture_cache.c's one stolen block */
static boolean map_precached;     /* the map being loaded is already read */
static boolean map_loaded;
static int menu_loads, movie_closes, precache_begins;
static long global_scenario_index = 0, global_structure_bsp_index = 0;
static void *global_scenario, *global_structure_bsp, *global_collision_bsp, *global_bsp3d, *global_game_globals;
static struct { long structure_bsp_index; } scenario_globals_storage, *scenario_globals = &scenario_globals_storage;
static struct game_runtime_globals_prefix { boolean map_load_in_progress; real loading_progress; } game_globals_storage,
	*game_globals = &game_globals_storage;
static short player_spawn_count = 1;

void platform_log(const char *format, ...) { (void)format; }
static long platform_input_press_count(void) { return 0; }
static void event_manager_suppress(boolean suppress) { (void)suppress; }
static void BinkClose(HBINK bink) { (void)bink; movie_closes++; }
static void bink_free_texture_cache(void) { stolen_memory = FALSE; }
static void main_menu_load(void) { menu_loads++; }
static void attract_mode_reset_timer(void) {}

static boolean cache_files_precache_map_loaded(char *name) { (void)name; return map_precached; }
static boolean cache_files_precache_in_progress(void) { return precache_begins && !map_precached; }
static boolean cache_files_precache_is_copying_map(char *name) { (void)name; return TRUE; }
static void cache_files_precache_map_end(void) { map_precached = TRUE; }
static void cache_files_precache_map_queue_end(void) {}
static boolean cache_files_precache_map_begin(char *name, boolean blocking)
{
	(void)name; (void)blocking;
	CHECK(!stolen_memory, "the precache took the texture cache's memory while a movie held it");
	precache_begins++;
	return TRUE;
}
static long cache_files_precache_map_status(real *progress) { *progress = 1.0f; return 1; }
static void cache_files_precache_set_priority(boolean blocking) { (void)blocking; }
static void main_queue_map_name(char *name) { (void)name; }
static void error(int kind, char const *format, ...) { (void)kind; (void)format; }
static void ui_widget_load_progress_widget(void) {}
static void progress_bar_begin(boolean b) { (void)b; }
static void progress_bar_end(void) {}
static void main_pregame_render(void) {}
static void main_rasterizer_throttle(void) {}
static void main_present_frame(void) {}
static void ui_widgets_close_all(void) {}
static void display_error_damaged_media(void) {}
static void main_save_current_solo_map(char *name) { (void)name; }
static void player_ui_remember_player1_profile(boolean b) { (void)b; }
static void structure_culling_dispose(void) {}
static void scenario_tags_unload(void) { map_loaded = FALSE; }

#include "under_test.inc"

static void play_intro(void)
{
	static struct { long FrameNum, Frames; } movie = { 100, 481 };

	bink_globals.initialized = TRUE;
	bink_globals.bink = (HBINK)&movie;
	/* ui_widget.c's startup intro */
	bink_globals.flags = FLAG(_bink_playback_button_click_stops_movie_bit) |
		FLAG(_bink_playback_prevent_events_to_ui_bit) |
		FLAG(_bink_playback_return_to_main_menu_when_finished_bit) |
		FLAG(_bink_playback_dont_allow_skipping_if_filesystem_check_thread_is_active_bit) |
		FLAG(_bink_playback_eat_up_memory_like_a_goddamn_beaver_bit);
	stolen_memory = TRUE;
}

/* main.c's main_change_map_name and network_game_manager.c's load: the
precache, then the old map's unload */
static void change_map(void)
{
	static char map_name[] = "levels\\a10\\a10";

	game_precache_new_map(map_name, TRUE);
	map_loaded = TRUE;
	scenario_unload();
}

int main(int argc, char **argv)
{
	const char *c = argc > 1 ? argv[1] : "";

	bink_globals.initialized = TRUE;
	if (!strcmp(c, "intro"))
	{
		play_intro();
		change_map();
		CHECK(!bink_playback_active(), "the movie still plays");
		CHECK(movie_closes == 1, "closed %d times", movie_closes);
		CHECK(!stolen_memory, "the movie kept the texture cache's memory");
		CHECK(menu_loads == 0, "the main menu loaded in the middle of a map load");
		CHECK(precache_begins == 1, "precache began %d times", precache_begins);
	}
	else if (!strcmp(c, "intro-map-already-read"))
	{
		play_intro();
		map_precached = TRUE;
		change_map();
		CHECK(!bink_playback_active(), "the movie still plays");
		CHECK(menu_loads == 0, "the main menu loaded in the middle of a map load");
	}
	else if (!strcmp(c, "no-movie"))
	{
		change_map();
		CHECK(movie_closes == 0, "closed %d times", movie_closes);
		CHECK(menu_loads == 0, "the main menu loaded");
		CHECK(precache_begins == 1, "precache began %d times", precache_begins);
	}
	else if (!strcmp(c, "skip"))
	{
		/* a player's skip still returns to the main menu */
		play_intro();
		bink_playback_stop();
		CHECK(menu_loads == 1, "a skipped intro loaded the main menu %d times", menu_loads);
	}
	else
	{
		fprintf(stderr, "unknown case %s\n", c);
		return 2;
	}
	return 0;
}
