/*
CONSOLE_CONFIG.C

The script console's own commands, one table (console_commands) behind one
entry point (console_config_command), checked before a line goes to the
script compiler. Both are development builds only (host_development_hooks): a
distribution build answers that the command is not available and changes
nothing.

`visr_set SECTION.KEY VALUE` sets one
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

`visr_cinematic` answers "visr_cinematic: in progress N, skippable M": whether
a cinematic runs, and whether this machine may ask to skip it. `visr_cinematic
skip` asks, through the one skip request the player's button makes
(cinematic_skip.c), and adds what came of it: ", skip requested" (the game
skips at the end of the frame), ", skip voted" (network co-op), or ", skip
declined: " and why. Scripted captures wait for "in progress 0"; the reply's
"visr_cinematic: in progress N" prefix is what they parse, so it stays.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cinematic_skip.h"
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
#define CONSOLE_CINEMATIC_COMMAND "visr_cinematic"

static const char *skip_spaces(const char *text)
{
	while (*text == ' ' || *text == '\t')
		text++;
	return text;
}

/* visr_set: arguments are what follows the command's name */
static void console_set_command(const char *arguments, char *reply, unsigned long reply_size)
{
	const char *cursor = skip_spaces(arguments);
	char name[96], value[260];
	size_t length;

	for (length = 0; cursor[length] && cursor[length] != ' ' && cursor[length] != '=' && cursor[length] != '\t'; length++)
		;
	if (!length || length >= sizeof(name))
	{
		snprintf(reply, reply_size, CONSOLE_CONFIG_COMMAND ": usage: " CONSOLE_CONFIG_COMMAND " SECTION.KEY VALUE");
		return;
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
		return;
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
}

/* visr_cinematic [skip] */
static void console_cinematic_command(const char *arguments, char *reply, unsigned long reply_size)
{
	static const char *const declined[] = {
		[CINEMATIC_SKIP_NO_CINEMATIC] = "no cinematic",
		[CINEMATIC_SKIP_NOT_SKIPPABLE] = "it cannot be skipped now",
		[CINEMATIC_SKIP_REPLAY_OWNS] = "a replay owns skips",
		[CINEMATIC_SKIP_PHOTO_MODE] = "photo mode is on",
	};
	const char *cursor = skip_spaces(arguments);
	size_t length = strlen(cursor);
	const char *outcome = "";
	enum cinematic_skip_result result = CINEMATIC_SKIP_NO_CINEMATIC;
	int in_progress, skippable, skip;

	while (length && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t'))
		length--;
	skip = length == 4 && !strncmp(cursor, "skip", 4);
	if (length && !skip)
	{
		snprintf(reply, reply_size, CONSOLE_CINEMATIC_COMMAND ": usage: " CONSOLE_CINEMATIC_COMMAND " [skip]");
		return;
	}
	if (skip)
	{
		result = cinematic_skip_request(CINEMATIC_SKIP_PLAYER);
		outcome = result == CINEMATIC_SKIP_REQUESTED ? ", skip requested" : result == CINEMATIC_SKIP_VOTED ? ", skip voted" : "";
	}
	in_progress = cinematic_skip_state(&skippable);
	if (skip && result != CINEMATIC_SKIP_REQUESTED && result != CINEMATIC_SKIP_VOTED)
		snprintf(reply, reply_size, CONSOLE_CINEMATIC_COMMAND ": in progress %d, skippable %d, skip declined: %s",
			in_progress, skippable, (unsigned)result < sizeof(declined) / sizeof(declined[0]) && declined[result] ?
			declined[result] : "unknown");
	else
		snprintf(reply, reply_size, CONSOLE_CINEMATIC_COMMAND ": in progress %d, skippable %d%s", in_progress, skippable,
			outcome);
}

/* the console's commands: name, and the handler given what follows the name; every one is development only */
static const struct
{
	const char *name;
	void (*handler)(const char *arguments, char *reply, unsigned long reply_size);
} console_commands[] = {
	{ CONSOLE_CONFIG_COMMAND, console_set_command },
	{ CONSOLE_CINEMATIC_COMMAND, console_cinematic_command },
};

int console_config_command(const char *line, char *reply, unsigned long reply_size)
{
	const char *cursor = skip_spaces(line);
	size_t index;

	for (index = 0; index < sizeof(console_commands) / sizeof(console_commands[0]); index++)
	{
		size_t length = strlen(console_commands[index].name);

		if (strncmp(cursor, console_commands[index].name, length) ||
			(cursor[length] != ' ' && cursor[length] != '\t' && cursor[length] != 0))
			continue;
		if (!host_development_hooks())
			snprintf(reply, reply_size, "%s: not available in this build", console_commands[index].name);
		else
			console_commands[index].handler(cursor + length, reply, reply_size);
		return 1;
	}
	return 0;
}
