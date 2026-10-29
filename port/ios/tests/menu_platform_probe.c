/* Menu capabilities follow the host; the retired quit platform is rejected. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../linux/src/menu_platform.h"

static int failures;

static void check(char const *platform, int expected)
{
	int got = menu_platform_matches(platform);

	printf("%s: \"%s\" is %d, expected %d\n", got == expected ? "ok" : "FAIL", platform, got, expected);
	if (got != expected)
		failures++;
}

int main(int argc, char **argv)
{
	/* argv[1]: "desktop", "apple", "mac" or "android" */
	char const *kind = argc > 1 ? argv[1] : "";
#if defined(HALO_ANDROID)
	int want_desktop = 0, want_android = 1;
#elif defined(HALO_ILP32)
	int want_desktop = 0, want_android = 1;
#else
	int want_desktop = 1, want_android = 0;
#endif
#if defined(HALO_IOS)
	check("cloud_saves", getenv("HALO_HOST_CLOUD_SAVES") && !strcmp(getenv("HALO_HOST_CLOUD_SAVES"), "1"));
#else
	check("cloud_saves", 0);
#endif
#if defined(HALO_IOS)
	check("tvos", getenv("HALO_HOST_TVOS") && !strcmp(getenv("HALO_HOST_TVOS"), "1"));
#else
	check("tvos", 0);
#endif
#if defined(HALO_ANDROID)
	check("keyboard", 0);
#elif defined(HALO_ILP32)
	check("keyboard", getenv("HALO_HOST_KEYBOARD") && !strcmp(getenv("HALO_HOST_KEYBOARD"), "1"));
#else
	check("keyboard", 1);
#endif
	check("quit", -1);
	check("desktop", want_desktop);
	check("android", want_android);
	check("tv", -1);
	check("", -1);
	printf("%s (%s)\n", failures ? "menu platform probe FAILED" : "menu platform probe passed", kind);
	return failures != 0;
}
