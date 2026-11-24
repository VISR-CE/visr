/* host_guest_environment.c: which HALO_ variables of a launch the game gets (bug class: a launch variable that never
reaches the game, or one that overrides what the host decides) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host_guest_environment.h"

static int notes_passed, notes_refused;
static void note(const char *entry, int passed)
{
	(void)entry;
	if (passed)
		notes_passed++;
	else
		notes_refused++;
}

static int has(const char **env, size_t count, const char *entry)
{
	for (size_t i = 0; i < count; i++)
		if (!strcmp(env[i], entry))
			return 1;
	return 0;
}

int main(void)
{
	char *launch[] = { "HALO_SCENE_PRECISION=16", "PATH=/usr/bin", "HALO_DATA_ROOT=/elsewhere", "HALO_GRAPHICS=enhanced",
		"HALO_APP_CAN_QUIT=1", "HALO_HOST_MAC=1", "HALO_DISPLAY_MAX_FPS=240", "HALO_TEST_INPUT=a", "HALO_RENDER=x",
		"HALO_RENDER_HEIGHT=720", "HALO_NOEQUALS", "HALO_=1", NULL };
	const char *env[16] = { "HALO_DATA_ROOT=/data", "HALO_RENDER_HEIGHT=1080" };
	struct host_guest_environment_result result;
	size_t count = host_guest_environment_pass_on(env, 2, 16, launch, &result, note);

	/* a setting given at launch reaches the game, with or without a benchmark suite */
	assert(has(env, count, "HALO_SCENE_PRECISION=16") && has(env, count, "HALO_GRAPHICS=enhanced"));
	assert(has(env, count, "HALO_TEST_INPUT=a"));
	/* the host's own values stay the host's: a name it set already, and the names it alone decides */
	assert(has(env, count, "HALO_DATA_ROOT=/data") && !has(env, count, "HALO_DATA_ROOT=/elsewhere"));
	assert(has(env, count, "HALO_RENDER_HEIGHT=1080") && !has(env, count, "HALO_RENDER_HEIGHT=720"));
	assert(!has(env, count, "HALO_APP_CAN_QUIT=1") && !has(env, count, "HALO_HOST_MAC=1") &&
		!has(env, count, "HALO_DISPLAY_MAX_FPS=240"));
	/* a name that is a prefix of a known one is its own variable; non-HALO_ and malformed entries are left out */
	assert(has(env, count, "HALO_RENDER=x"));
	assert(!has(env, count, "PATH=/usr/bin") && !has(env, count, "HALO_NOEQUALS") && !has(env, count, "HALO_=1"));
	assert(result.passed == 4 && result.refused == 3 && result.dropped == 0);
	assert(notes_passed == 4 && notes_refused == 3);
	assert(count == 2 + result.passed);
	/* past the capacity: counted as dropped, never written past the end */
	{
		const char *small[3] = { "HALO_DATA_ROOT=/data" };
		size_t n = host_guest_environment_pass_on(small, 1, 3, launch, &result, NULL);

		assert(n == 3 && result.passed == 2 && result.dropped == 3);
	}
	puts("guest_environment_probe: ok");
	return 0;
}
