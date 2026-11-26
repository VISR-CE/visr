/*
HALO_DEVELOPMENT_HOOKS.H

Whether this is a development build, the one place the non-Apple rule lives. The Apple hosts answer it themselves
(host_development_hooks in port/runtime/guest/runtime/guest_host.h, defined in port/ios/host); everywhere else a
development build is one started with HALO_DEVELOPMENT_HOOKS=1 (anything else, "true" and "01" included, is not).
Include it by relative path (../include/halo_development_hooks.h): the game's units and the platform's search
different directories.
*/

#ifndef HALO_DEVELOPMENT_HOOKS_H
#define HALO_DEVELOPMENT_HOOKS_H

#ifdef HALO_ILP32
extern int host_development_hooks(void);
#else
#include <stdlib.h>
#include <string.h>

static inline int host_development_hooks(void)
{
	const char *value = getenv("HALO_DEVELOPMENT_HOOKS");
	return value && !strcmp(value, "1");
}
#endif

#endif
