/* texture_upscale_schedule.h: the profile gate (rule 1) and when the work may run (rules 4 and 6) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "texture_upscale_schedule.h"

static void gate(void)
{
	assert(texture_upscale_mode_parse(NULL) == TEXTURE_UPSCALE_MODE_OFF);
	assert(texture_upscale_mode_parse("off") == TEXTURE_UPSCALE_MODE_OFF);
	/* what 0.5.x left in config.toml (the old boolean) and anything unrecognized is auto, as the guest's reader has it */
	assert(texture_upscale_mode_parse("false") == TEXTURE_UPSCALE_MODE_AUTO);
	assert(texture_upscale_mode_parse("0") == TEXTURE_UPSCALE_MODE_AUTO);
	assert(texture_upscale_mode_parse("") == TEXTURE_UPSCALE_MODE_AUTO);
	assert(texture_upscale_mode_parse("nonsense") == TEXTURE_UPSCALE_MODE_AUTO);
	assert(texture_upscale_mode_parse("auto") == TEXTURE_UPSCALE_MODE_AUTO);
	assert(texture_upscale_mode_parse("true") == TEXTURE_UPSCALE_MODE_AUTO);
	assert(texture_upscale_mode_parse("force") == TEXTURE_UPSCALE_MODE_FORCE);
	/* the profile is asked first: the player's switch never turns it on where the profile has it off */
	assert(texture_upscale_gate(TEXTURE_UPSCALE_MODE_AUTO, 1, 1, 1));
	assert(!texture_upscale_gate(TEXTURE_UPSCALE_MODE_AUTO, 0, 1, 1));
	assert(!texture_upscale_gate(TEXTURE_UPSCALE_MODE_AUTO, 1, 0, 1));
	assert(!texture_upscale_gate(TEXTURE_UPSCALE_MODE_OFF, 1, 1, 1));
	assert(texture_upscale_gate(TEXTURE_UPSCALE_MODE_FORCE, 0, 0, 1));
	/* no Neural Engine, nothing, even forced (rule 8) */
	assert(!texture_upscale_gate(TEXTURE_UPSCALE_MODE_FORCE, 1, 1, 0));
	assert(!texture_upscale_gate(TEXTURE_UPSCALE_MODE_AUTO, 1, 1, 0));
}

static void activity(void)
{
	struct texture_upscale_activity a = { 0 };

	/* menus: free to run */
	assert(texture_upscale_activity_may_run(&a, 0) && texture_upscale_activity_wait(&a, 0) == 0);
	/* a level's game: never, until something changes */
	texture_upscale_activity_game_started(&a, 1);
	assert(!texture_upscale_activity_may_run(&a, 10) && texture_upscale_activity_wait(&a, 10) < 0);
	/* paused: only after TEXTURE_UPSCALE_PAUSE_SECONDS */
	texture_upscale_activity_paused(&a, 1, 20);
	assert(!texture_upscale_activity_may_run(&a, 21));
	assert(texture_upscale_activity_wait(&a, 21) > 0.9 && texture_upscale_activity_wait(&a, 21) < 1.1);
	assert(texture_upscale_activity_may_run(&a, 22));
	/* a second pause signal does not restart the clock */
	texture_upscale_activity_paused(&a, 1, 21.5);
	assert(texture_upscale_activity_may_run(&a, 22));
	/* resumed: stops at once */
	texture_upscale_activity_paused(&a, 0, 23);
	assert(!texture_upscale_activity_may_run(&a, 100));
	/* the next map's load, and the menus' map, run again */
	texture_upscale_activity_map_will_load(&a);
	assert(texture_upscale_activity_may_run(&a, 100));
	texture_upscale_activity_game_started(&a, 1);
	texture_upscale_activity_game_started(&a, 0);
	assert(texture_upscale_activity_may_run(&a, 100));
}

static void order_and_skip(void)
{
	/* the main menu's map is first, then the campaign in order, then multiplayer maps, then community maps */
	assert(texture_upscale_map_rank("ui", 0) < texture_upscale_map_rank("a10", 0));
	assert(texture_upscale_map_rank("a10", 0) < texture_upscale_map_rank("d40", 0));
	assert(texture_upscale_map_rank("d40", 0) < texture_upscale_map_rank("bloodgulch", 0));
	assert(texture_upscale_map_rank("bloodgulch", 0) < texture_upscale_map_rank("anything", 1));
	assert(texture_upscale_map_rank("ui", 0) == 0);
	/* Skip waits for the menu's textures: not before the catalog lists them, not while any is missing */
	assert(!texture_upscale_import_skippable(0, 0, 0));
	assert(!texture_upscale_import_skippable(1, 0, 40));
	assert(!texture_upscale_import_skippable(1, 39, 40));
	assert(texture_upscale_import_skippable(1, 40, 40));
	/* nothing to upscale in the menu map (or no menu map): nothing to wait for */
	assert(texture_upscale_import_skippable(1, 0, 0));
}

static void level_source(void)
{
	/* a launch straight into a level: the catalog is not built yet, the level still reads its own list */
	assert(texture_upscale_level_source(1, 0) == TEXTURE_UPSCALE_LEVEL_SOURCE_DISK);
	assert(texture_upscale_level_source(0, 0) == TEXTURE_UPSCALE_LEVEL_SOURCE_DISK);
	assert(texture_upscale_level_source(0, 1) == TEXTURE_UPSCALE_LEVEL_SOURCE_DISK);
	assert(texture_upscale_level_source(1, 1) == TEXTURE_UPSCALE_LEVEL_SOURCE_CATALOG);
	/* the loading screen's upscaling line: only while textures are being made; never for a ready level or a listing */
	assert(!texture_upscale_level_note_visible(TEXTURE_UPSCALE_LEVEL_NONE, 0));
	assert(!texture_upscale_level_note_visible(TEXTURE_UPSCALE_LEVEL_NONE, 5000));
	assert(!texture_upscale_level_note_visible(TEXTURE_UPSCALE_LEVEL_CHECKING, 0));
	assert(!texture_upscale_level_note_visible(TEXTURE_UPSCALE_LEVEL_WORKING, 0));
	assert(!texture_upscale_level_note_visible(TEXTURE_UPSCALE_LEVEL_DONE, 5000));
	assert(texture_upscale_level_note_visible(TEXTURE_UPSCALE_LEVEL_WORKING, 5000));
	/* the import screen's one-time bar for an already-imported game */
	assert(texture_upscale_first_run_phase(1, 0, 0));
	assert(!texture_upscale_first_run_phase(0, 0, 0));   /* the profile or switch has upscaling off */
	assert(!texture_upscale_first_run_phase(1, 1, 0));   /* shown once already */
	assert(!texture_upscale_first_run_phase(1, 0, 1));   /* nothing left to make */
}

static void import_watchdog(void)
{
	/* the menu's textures made: offered at once */
	assert(texture_upscale_import_offer_skip(1, 182, 182, 0));
	/* still making them and moving: held back */
	assert(!texture_upscale_import_offer_skip(1, 90, 182, 5));
	assert(!texture_upscale_import_offer_skip(0, 0, 0, TEXTURE_UPSCALE_IMPORT_STALL_SECONDS - 1));
	/* no step finished for the stall time, before the catalog or during the menu's textures: offered anyway */
	assert(texture_upscale_import_offer_skip(0, 0, 0, TEXTURE_UPSCALE_IMPORT_STALL_SECONDS));
	assert(texture_upscale_import_offer_skip(1, 90, 182, TEXTURE_UPSCALE_IMPORT_STALL_SECONDS + 30));
	/* a level's wait while nothing lists its textures ends rather than holding the loading screen */
	assert(texture_upscale_level_checking(0) == TEXTURE_UPSCALE_LEVEL_CHECKING);
	assert(texture_upscale_level_checking(TEXTURE_UPSCALE_LEVEL_CHECK_SECONDS - 1) == TEXTURE_UPSCALE_LEVEL_CHECKING);
	assert(texture_upscale_level_checking(TEXTURE_UPSCALE_LEVEL_CHECK_SECONDS) == TEXTURE_UPSCALE_LEVEL_DONE);
}

/* A model of the pump's end: how many pumps run back to back after one is scheduled, when every pump ends the same
   way and a scan stays pending (as when a gate refuses before the scan is taken). 0.6.3's storage refusal ran without
   end; any refusal now runs once. */
static int pumps_in_a_row(enum texture_upscale_end end)
{
	int runs = 0;

	do
		runs++;
	while (runs < 1000 && texture_upscale_pump_runs_again(end, 1, 0, 0, 1));
	return runs;
}

static void refusals(void)
{
	int e;

	/* every refusal ends the pump, whatever is pending: the 0.6.3 loop cannot come back for any of them */
	for (e = 0; e < TEXTURE_UPSCALE_END_COUNT; e++)
		if (texture_upscale_end_refused((enum texture_upscale_end)e))
		{
			assert(pumps_in_a_row((enum texture_upscale_end)e) == 1);
			assert(!texture_upscale_pump_runs_again((enum texture_upscale_end)e, 0, 1, 0, 1));
		}
	/* exactly these four are refusals */
	assert(texture_upscale_end_refused(TEXTURE_UPSCALE_END_REFUSED_STORAGE));
	assert(texture_upscale_end_refused(TEXTURE_UPSCALE_END_REFUSED_SWITCH));
	assert(texture_upscale_end_refused(TEXTURE_UPSCALE_END_REFUSED_THERMAL));
	assert(texture_upscale_end_refused(TEXTURE_UPSCALE_END_REFUSED_GAMEPLAY));
	assert(!texture_upscale_end_refused(TEXTURE_UPSCALE_END_QUEUE_EMPTY));
	assert(!texture_upscale_end_refused(TEXTURE_UPSCALE_END_INTERRUPTED));
	assert(!texture_upscale_end_refused(TEXTURE_UPSCALE_END_ERROR));
	/* a pump that ended for its own reasons runs again for a pending scan or request, not in gameplay or when too warm */
	assert(texture_upscale_pump_runs_again(TEXTURE_UPSCALE_END_INTERRUPTED, 1, 0, 0, 1));
	assert(texture_upscale_pump_runs_again(TEXTURE_UPSCALE_END_QUEUE_EMPTY, 0, 1, 0, 1));
	assert(!texture_upscale_pump_runs_again(TEXTURE_UPSCALE_END_QUEUE_EMPTY, 0, 0, 0, 1));
	assert(!texture_upscale_pump_runs_again(TEXTURE_UPSCALE_END_INTERRUPTED, 1, 0, 1, 1));
	assert(!texture_upscale_pump_runs_again(TEXTURE_UPSCALE_END_INTERRUPTED, 1, 0, 0, 0));
	/* storage and errors retry on a timer; the others wait for their notice */
	assert(texture_upscale_end_retries_on_timer(TEXTURE_UPSCALE_END_REFUSED_STORAGE));
	assert(texture_upscale_end_retries_on_timer(TEXTURE_UPSCALE_END_ERROR));
	assert(!texture_upscale_end_retries_on_timer(TEXTURE_UPSCALE_END_REFUSED_THERMAL));
	assert(!texture_upscale_end_retries_on_timer(TEXTURE_UPSCALE_END_REFUSED_GAMEPLAY));
	/* every end has a name */
	for (e = 0; e < TEXTURE_UPSCALE_END_COUNT; e++)
		assert(strcmp(texture_upscale_end_name((enum texture_upscale_end)e), "unknown"));
	/* what the import screen says when the phase ends refused, and only then */
	assert(!strcmp(texture_upscale_import_end_message(TEXTURE_UPSCALE_END_REFUSED_STORAGE), TEXTURE_UPSCALE_IMPORT_NO_STORAGE));
	assert(!strcmp(texture_upscale_import_end_detail(TEXTURE_UPSCALE_END_REFUSED_STORAGE), TEXTURE_UPSCALE_IMPORT_NO_STORAGE_DETAIL));
	assert(!strcmp(texture_upscale_import_end_message(TEXTURE_UPSCALE_END_REFUSED_THERMAL), TEXTURE_UPSCALE_IMPORT_TOO_WARM));
	assert(!texture_upscale_import_end_message(TEXTURE_UPSCALE_END_QUEUE_EMPTY));
	assert(!texture_upscale_import_end_message(TEXTURE_UPSCALE_END_ERROR));
	assert(!texture_upscale_import_end_detail(TEXTURE_UPSCALE_END_INTERRUPTED));
}

static void import_retry(void)
{
	for (int e = 0; e < TEXTURE_UPSCALE_END_COUNT; e++)
	{
		enum texture_upscale_end end = (enum texture_upscale_end)e;
		int offered = end == TEXTURE_UPSCALE_END_REFUSED_STORAGE || end == TEXTURE_UPSCALE_END_REFUSED_THERMAL;
		assert(texture_upscale_import_retry_available(end) == offered);
		assert(texture_upscale_import_retry_outcome(end, 1) == offered);
		assert(!texture_upscale_import_retry_outcome(end, 0));
		assert((texture_upscale_import_retry_note(end) != NULL) == offered);
	}
	assert(!strcmp(texture_upscale_import_retry_note(TEXTURE_UPSCALE_END_REFUSED_STORAGE), "Still not enough free storage."));
	assert(!strcmp(texture_upscale_import_retry_note(TEXTURE_UPSCALE_END_REFUSED_THERMAL), "Still too warm."));
}

int main(void)
{
	import_retry();
	refusals();
	import_watchdog();
	level_source();
	gate();
	activity();
	order_and_skip();
	puts("texture_upscale_schedule_probe: ok");
	return 0;
}
