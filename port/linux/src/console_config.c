/*
CONSOLE_CONFIG.C

The script console's `visr_set SECTION.KEY VALUE` line, which sets one
setting for this run only through config_set_live (nothing is written), so a
look comparison can switch a setting live and shoot every value at the same
frozen tick instead of relaunching once per value. Development builds only
(host_development_hooks): a distribution build answers that the command is
not available and changes nothing.

VALUE is the rest of the line, written as in a settings file: a quoted
string loses its quotes (display.anti_aliasing "smaa"), and an `=` between
the key and the value is accepted (display.shadow_map_size=256).

The reply is the value in force, read back after the set
("visr_set: display.shadow_map_size = 256"), or "visr_set: refused: " and
why: no such setting, a launch-only one, a value of the wrong type or not
one of the setting's choices, or a value that does not take (the graphics
mode replaces the setting, for one).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console_config.h"

int config_set_live(const char *name, const char *value, char *note, size_t note_size);

#ifdef HALO_ILP32
/* port/runtime/guest/runtime/guest_host.h */
extern int host_development_hooks(void);
#else
static int host_development_hooks(void)
{
	const char *value = getenv("HALO_DEVELOPMENT_HOOKS");
	return value && !strcmp(value, "1");
}
#endif

#define CONSOLE_CONFIG_COMMAND "visr_set"

static const char *skip_spaces(const char *text)
{
	while (*text == ' ' || *text == '\t')
		text++;
	return text;
}

int console_config_command(const char *line, char *reply, unsigned long reply_size)
{
	const char *cursor = skip_spaces(line);
	char name[96], value[260];
	size_t length;

	if (strncmp(cursor, CONSOLE_CONFIG_COMMAND, strlen(CONSOLE_CONFIG_COMMAND)) ||
		(cursor[strlen(CONSOLE_CONFIG_COMMAND)] != ' ' && cursor[strlen(CONSOLE_CONFIG_COMMAND)] != 0))
		return 0;
	if (!host_development_hooks())
	{
		snprintf(reply, reply_size, CONSOLE_CONFIG_COMMAND ": not available in this build");
		return 1;
	}
	cursor = skip_spaces(cursor + strlen(CONSOLE_CONFIG_COMMAND));
	for (length = 0; cursor[length] && cursor[length] != ' ' && cursor[length] != '=' && cursor[length] != '\t'; length++)
		;
	if (!length || length >= sizeof(name))
	{
		snprintf(reply, reply_size, CONSOLE_CONFIG_COMMAND ": usage: " CONSOLE_CONFIG_COMMAND " SECTION.KEY VALUE");
		return 1;
	}
	memcpy(name, cursor, length);
	name[length] = 0;
	cursor = skip_spaces(cursor + length);
	if (*cursor == '=')
		cursor = skip_spaces(cursor + 1);
	length = strlen(cursor);
	while (length && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t'))
		length--;
	if (length >= 2 && cursor[0] == '"' && cursor[length - 1] == '"')
	{
		cursor++;
		length -= 2;
	}
	if (!*cursor || length >= sizeof(value))
	{
		snprintf(reply, reply_size, CONSOLE_CONFIG_COMMAND ": usage: " CONSOLE_CONFIG_COMMAND " SECTION.KEY VALUE");
		return 1;
	}
	memcpy(value, cursor, length);
	value[length] = 0;
	{
		char note[400];

		if (config_set_live(name, value, note, sizeof(note)))
			snprintf(reply, reply_size, CONSOLE_CONFIG_COMMAND ": %s", note);
		else
			snprintf(reply, reply_size, CONSOLE_CONFIG_COMMAND ": refused: %s", note);
	}
	return 1;
}
