/* See host_guest_environment.h. */
#include "host_guest_environment.h"
#include <string.h>

static size_t name_length(const char *entry)
{
	const char *equals = strchr(entry, '=');

	return equals ? (size_t)(equals - entry) : 0;
}

static int name_is(const char *entry, size_t length, const char *name)
{
	return strlen(name) == length && !strncmp(entry, name, length);
}

int host_guest_environment_host_owned(const char *entry)
{
	static const char *const prefixes[] = { "HALO_HOST_", "HALO_APP_" };
	static const char *const names[] = { "HALO_DISPLAY_MAX_FPS", "HALO_DEVICE_PROFILE_VALUES", "HALO_DATA_ROOT",
		"HALO_SAVE_ROOT", "HALO_DISPLAY_WIDTH", "HALO_DISPLAY_PIXEL_WIDTH", "HALO_DISPLAY_PIXEL_HEIGHT",
		"HALO_PHOTO_MODE", "HALO_MAP_PICTURES_RENDER" };
	size_t length = name_length(entry), i;

	for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
		if (!strncmp(entry, prefixes[i], strlen(prefixes[i])))
			return 1;
	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (name_is(entry, length, names[i]))
			return 1;
	return 0;
}

size_t host_guest_environment_pass_on(const char **env, size_t count, size_t capacity, char *const *launch,
	struct host_guest_environment_result *result, void (*note)(const char *entry, int passed))
{
	struct host_guest_environment_result counts = { 0, 0, 0 };

	for (; launch && *launch; launch++)
	{
		const char *entry = *launch;
		size_t length = name_length(entry), i;
		int known = 0;

		if (strncmp(entry, "HALO_", 5) || length <= 5)
			continue;
		for (i = 0; i < count && !known; i++)
			known = !strncmp(env[i], entry, length) && env[i][length] == '=';
		if (known)
			continue;
		if (host_guest_environment_host_owned(entry))
		{
			counts.refused++;
			if (note)
				note(entry, 0);
			continue;
		}
		if (count >= capacity)
		{
			counts.dropped++;
			continue;
		}
		env[count++] = entry;
		counts.passed++;
		if (note)
			note(entry, 1);
	}
	if (result)
		*result = counts;
	return count;
}
