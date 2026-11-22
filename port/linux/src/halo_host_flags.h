/* The Apple host's flags to the game: environment variables it sets to "1"
or "0" before the game starts (port/ios/host/host_main.m), such as
HALO_HOST_MAC, HALO_HOST_TVOS and HALO_HOST_VISIONOS. The game is built once
for every Apple device, so these, not the compiler, say which device it runs
on. Every reader goes through here; tools/test_host_flags.py fails on another
read of HALO_HOST_VISIONOS. */
#ifndef HALO_HOST_FLAGS_H
#define HALO_HOST_FLAGS_H

#include <stdlib.h>

/* the flag is set, to exactly "1" */
static inline int halo_host_flag(const char *name)
{
	const char *value = getenv(name);

	return value && value[0] == '1' && value[1] == 0;
}

/* the game runs on Apple Vision Pro */
static inline int halo_host_visionos(void)
{
	return halo_host_flag("HALO_HOST_VISIONOS");
}

#endif
