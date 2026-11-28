/* HEAD mode's help text (port/linux/game/stereo_help_text.c, included, not
linked): a10's look lessons drop the right stick's look prompt.

Checks, with display.stereo "head" and "side_by_side": a10's look-only
prompts (tutorial_looking_1 and _2, "Use <look> to look ...", and the
southpaw layout's _1l and _2l) aren't shown; tutorial_moving_1 ("Use <move>
to move|n|nUse <look> to look") shows its first line only; every other
message, the southpaw tutorial_moving_1l (moving and sidestepping) and the
lessons that don't name a stick (tutorial_looking_targeted_*, ladder_1)
among them, shows whole. With "off", "screen", an unrecognized mode or none,
every message shows whole, so mono's frames are unchanged. And the first
line's end: the characters before the first "|n", or the whole text (up to
its terminator) when it has none. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../linux/game/stereo_help_text.c"

static const char *probe_stereo;
/* fake: answers display.stereo as the probe sets it per case */
const char *config_string(const char *name)
{
	return !strcmp(name, "display.stereo") ? probe_stereo : NULL;
}

long game_time_get(void)
{
	return 0;
}

static int log_lines;
/* fake: prints the log lines and counts them for the checks */
void platform_log(const char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	printf("\n");
	log_lines++;
}

static int failures;

static void check(int condition, const char *what)
{
	printf("%s: %s\n", condition ? "ok" : "FAILED", what);
	if (!condition)
		failures++;
}

/* text as the tag stores it: UTF-16 characters, a terminator counted in the element's length */
static int line_end(const char *ascii)
{
	unsigned short text[64];
	int length = (int)strlen(ascii) + 1;
	int i;

	for (i = 0; i < length; i++)
		text[i] = (unsigned char)ascii[i];
	return halo_stereo_help_text_line_end(text, length);
}

int main(void)
{
	static const char *const head_modes[] = {"head", "side_by_side"};
	static const char *const flat_modes[] = {"off", "screen", "theater", NULL};
	static const char *const hidden[] = {"tutorial_looking_1", "tutorial_looking_2", "tutorial_looking_1l",
		"tutorial_looking_2l"};
	static const char *const whole[] = {"tutorial_introduction_1", "tutorial_moving_1l", "tutorial_action_1",
		"tutorial_looking_targeted_2", "tutorial_looking_choose_2", "ladder_1", "tutorial_looking",
		"tutorial_looking_1x", ""};
	unsigned i, j;
	int lines_before;

	for (i = 0; i < 2; i++)
	{
		probe_stereo = head_modes[i];
		printf("display.stereo %s\n", probe_stereo);
		for (j = 0; j < sizeof(hidden) / sizeof(hidden[0]); j++)
			check(halo_stereo_help_text_part(hidden[j]) == HALO_HELP_TEXT_NONE, hidden[j]);
		check(halo_stereo_help_text_part("tutorial_moving_1") == HALO_HELP_TEXT_FIRST_LINE,
			"tutorial_moving_1 shows its first line (the move) only");
		for (j = 0; j < sizeof(whole) / sizeof(whole[0]); j++)
			check(halo_stereo_help_text_part(whole[j]) == HALO_HELP_TEXT_ALL, whole[j]);
		check(halo_stereo_help_text_part(NULL) == HALO_HELP_TEXT_ALL, "no message shows whole");
	}
	for (i = 0; i < sizeof(flat_modes) / sizeof(flat_modes[0]); i++)
	{
		probe_stereo = flat_modes[i];
		printf("display.stereo %s\n", probe_stereo ? probe_stereo : "(none)");
		for (j = 0; j < sizeof(hidden) / sizeof(hidden[0]); j++)
			check(halo_stereo_help_text_part(hidden[j]) == HALO_HELP_TEXT_ALL, hidden[j]);
		check(halo_stereo_help_text_part("tutorial_moving_1") == HALO_HELP_TEXT_ALL, "tutorial_moving_1 whole");
	}

	/* the log: once as a filtered message first draws, not each frame */
	probe_stereo = "head";
	halo_stereo_help_text_part("tutorial_introduction_1");
	lines_before = log_lines;
	halo_stereo_help_text_part("tutorial_moving_1");
	halo_stereo_help_text_part("tutorial_moving_1");
	halo_stereo_help_text_part("tutorial_moving_1");
	check(log_lines == lines_before + 1, "a filtered message logs once while it stays up");

	/* tutorial_moving_1's third text element, and others */
	check(line_end(" to move|n|nUse ") == 8, "the first line of \" to move|n|nUse \" is \" to move\"");
	check(line_end("Use ") == 5, "a text without |n is all of it, its terminator included");
	check(line_end("|nUse ") == 0, "a text that begins with |n has an empty first line");
	check(line_end("a|") == 3, "a lone | at the end isn't a line break");
	check(line_end("50|% of it") == 11, "| followed by something other than n isn't a line break");

	printf("%d failures\n", failures);
	return failures != 0;
}
