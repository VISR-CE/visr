/* The game's environment from the launch's (host_main.m): which HALO_ variables of the app's own environment the guest
gets. Plain C, tested by port/ios/tests/guest_environment_probe.c. */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct host_guest_environment_result
{
	size_t passed;   /* launch variables added */
	size_t refused;  /* host-owned names (host_guest_environment_host_owned) */
	size_t dropped;  /* past the capacity */
};
/* Whether a NAME=VALUE entry's name is one the host alone sets or decides (the host-to-game protocol: HALO_HOST_*,
HALO_APP_*, the display and profile values, the data folders; and the development-build switches host_main.m passes
on itself): a launch never passes it on, so it cannot claim, say, that an iPhone app can quit. Settings variables and
HALO_TEST_ hooks are passed on. */
int host_guest_environment_host_owned(const char *entry);
/* Appends to env (count entries already, room for capacity) every HALO_ entry of launch (NULL-ended) whose name is not
in env already and not host-owned. Returns the new count; result (may be NULL) gets the counts. Keeps room for nothing
else: the caller passes the capacity it can give. note (may be NULL) is called for each entry passed (1) or refused (0). */
size_t host_guest_environment_pass_on(const char **env, size_t count, size_t capacity, char *const *launch,
	struct host_guest_environment_result *result, void (*note)(const char *entry, int passed));
#ifdef __cplusplus
}
#endif
