/*
STEREO_HELP_TEXT.C

HEAD mode's help text: a10's lessons without the right stick's look. In
HEAD mode the head turns and pitches the view, and players read "look
around" literally, so a prompt telling them to look with the right stick
misleads them. a10's scripts (its scenario's tutorial_looking and
tutorial_moving_1) set these help texts from levels\a10\hud messages, and no
other map's messages name the look or a stick:

  tutorial_looking_1   "Use <look> to look around"
  tutorial_looking_2   "Use <look> to look up and down"
  tutorial_looking_1l  "Use <left stick> to turn left and right|n|n
                        Use <right stick> to look up and down"
  tutorial_looking_2l  "Use <right stick> to look up and down"
  tutorial_moving_1    "Use <move> to move|n|nUse <look> to look"

(the "l" ones for the southpaw stick layout, player0_joystick_set_is_normal
false). The look lessons' prompts aren't shown at all, and the moving
lesson shows its first line, the move, alone. The southpaw
tutorial_moving_1l ("Use <left stick> to move forward and backward|n|nUse
<right stick> to sidestep left and right") is moving, which HEAD mode leaves
to the sticks, so it stays whole.

The scripts' waits need nothing: tutorial_looking waits for the look to move
left, right, up and down (player_action_test_look_relative_*), which the
head's turn sets as a stick's would (halo_stereo_head_look adds it to the
look's facing delta), and tutorial_moving_1 waits for the player to walk.

Only the draw changes (hud_messaging.c's hud_messaging_update asks here for
the help message it draws), not the help message the script set, so the
game state is the same in every mode. HEAD mode and the side-by-side view
(the Mac's stand-in for it) filter; every other display.stereo shows each
message whole.

port/ios/tests/stereo_help_text_probe.c includes this file.
*/

#include <string.h>

#include "../src/halo_stereo.h"

/* port_config.c's, the platform's and source/game/game_time.c's */
const char *config_string(const char *name);
void platform_log(const char *format, ...);
long game_time_get(void);

/* a10's help messages, by name, and how much of each HEAD mode shows */
static const struct
{
	const char *name;
	int part;
} help_text_parts[] =
{
	{"tutorial_looking_1", HALO_HELP_TEXT_NONE},
	{"tutorial_looking_2", HALO_HELP_TEXT_NONE},
	{"tutorial_looking_1l", HALO_HELP_TEXT_NONE},
	{"tutorial_looking_2l", HALO_HELP_TEXT_NONE},
	{"tutorial_moving_1", HALO_HELP_TEXT_FIRST_LINE},
};

/* the message the log last named, so a filtered message logs once while it stays up */
static char logged_name[32];

static int head_mode(void)
{
	const char *stereo = config_string("display.stereo");

	return stereo && (!strcmp(stereo, "head") || !strcmp(stereo, "side_by_side"));
}

int halo_stereo_help_text_part(const char *message_name)
{
	int part = HALO_HELP_TEXT_ALL;
	unsigned i;

	if (!message_name || !head_mode())
		return HALO_HELP_TEXT_ALL;
	for (i = 0; i < sizeof(help_text_parts) / sizeof(help_text_parts[0]); i++)
	{
		if (!strcmp(message_name, help_text_parts[i].name))
		{
			part = help_text_parts[i].part;
			break;
		}
	}
	if (part != HALO_HELP_TEXT_ALL && strncmp(logged_name, message_name, sizeof(logged_name)))
	{
		platform_log("stereo: help text %s: %s (game tick %ld)", message_name,
			part == HALO_HELP_TEXT_NONE ? "not shown (a look prompt)" : "its first line only (the move)",
			game_time_get());
	}
	if (part != HALO_HELP_TEXT_ALL || logged_name[0])
	{
		strncpy(logged_name, part != HALO_HELP_TEXT_ALL ? message_name : "", sizeof(logged_name) - 1);
		logged_name[sizeof(logged_name) - 1] = '\0';
	}
	return part;
}

int halo_stereo_help_text_line_end(const unsigned short *text, int length)
{
	int i;

	for (i = 0; i + 1 < length && text[i]; i++)
	{
		if (text[i] == '|' && text[i + 1] == 'n')
			return i;
	}
	return length;
}
