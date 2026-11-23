/*
PLAYER_INPUT_PROFILE.H

The input settings of a player profile that the Xbox profile record has no
room for: the mouse's and the keyboard's, and later the touch controls'. They
live in the profile file's former padding (see the codec's comment in
player_input_profile.c), so one profile carries every input setting, for each
local player, whatever the device.

The codec (decode, encode, defaults, field access) is plain C with no game
dependencies; port/ios/tests/player_input_profile_probe.c includes the .c.
*/

#ifndef PLAYER_INPUT_PROFILE_H
#define PLAYER_INPUT_PROFILE_H

#include <stddef.h>

/* where the extension sits in the profile file's 512-byte block (after the
base record, 0x30 bytes, and its 20-byte checksum), and its size */
#define PLAYER_INPUT_REGION_OFFSET 0x44
#define PLAYER_INPUT_REGION_SIZE (512 - PLAYER_INPUT_REGION_OFFSET)
#define PLAYER_INPUT_BINDING_ACTIONS 17
#define PLAYER_INPUT_BINDING_SLOTS 2
/* room for every byte of a payload this build does not understand: a region's whole payload */
#define PLAYER_INPUT_UNKNOWN_SIZE (PLAYER_INPUT_REGION_SIZE - 14)
#define PLAYER_INPUT_LOCAL_PLAYERS 4

/* a binding's stable code: the SDL scancode (the USB HID usage), 0x200 + the
mouse button, 0x300 the wheel, 0x301 wheel up, 0x302 wheel down; 0 for none.
The runtime's own input numbers (sdl_platform.h) change with SDL; these do
not, so they are what a profile stores */
#define PLAYER_INPUT_CODE_NONE 0
#define PLAYER_INPUT_CODE_MOUSE 0x200
#define PLAYER_INPUT_CODE_WHEEL 0x300
#define PLAYER_INPUT_CODE_WHEEL_UP 0x301
#define PLAYER_INPUT_CODE_WHEEL_DOWN 0x302

struct player_input_profile
{
	/* hundredths: 100 turns the view as the port's mouse always did */
	unsigned short mouse_sensitivity;
	/* hundredths; 0 turns it as fast as mouse_sensitivity */
	unsigned short mouse_vertical_sensitivity;
	unsigned char invert_mouse;
	unsigned char mouse_aim_assist;
	unsigned short bindings[PLAYER_INPUT_BINDING_ACTIONS][PLAYER_INPUT_BINDING_SLOTS];
};

/* what a profile file holds: the values, plus what a newer build wrote that
this one does not know (kept verbatim and written back, so a save here cannot
lose a newer build's settings) */
struct player_input_extension
{
	struct player_input_profile values;
	unsigned short container_version;
	unsigned short unknown_length;
	unsigned char unknown[PLAYER_INPUT_UNKNOWN_SIZE];
};

enum player_input_decode
{
	/* no extension (an older profile, or padding of zeros): the defaults */
	_player_input_absent,
	_player_input_present,
	/* the magic is there but the record is damaged: the defaults */
	_player_input_damaged,
	/* written by a build whose format this one cannot interpret: the values are the defaults and the region
	must be kept byte for byte, never rewritten */
	_player_input_newer
};

void player_input_profile_defaults(struct player_input_profile *values);
void player_input_extension_defaults(struct player_input_extension *extension);
/* region: PLAYER_INPUT_REGION_SIZE bytes of the file block at
PLAYER_INPUT_REGION_OFFSET. Always leaves *extension valid */
enum player_input_decode player_input_extension_decode(const unsigned char *region,
	struct player_input_extension *extension);
/* fills the whole region (the rest zero) */
void player_input_extension_encode(const struct player_input_extension *extension, unsigned char *region);

/* The profile file's next 512-byte block, as the write thread makes it, from the block now in the file (old_block, NULL
when it could not be read), a new record and checksum (base: the first PLAYER_INPUT_REGION_OFFSET bytes, NULL to keep
the file's), and new input values (input, NULL to keep the file's extension). Everything not replaced is kept byte for
byte, including what a newer build wrote. Returns 0 only when there is nothing to build on (no old block and no base) */
int player_input_block_merge(const unsigned char *old_block, const unsigned char *base,
	const struct player_input_profile *input, unsigned char *block);

/* a profile setting by name ("profile.mouse_sensitivity") as text, the way the
menus read and write them; 0 for a name that is none of these. Sensitivities
are decimal text ("1.25") */
int player_input_profile_is_setting(const char *name);
int player_input_profile_text(const struct player_input_profile *values, const char *name, char *text, size_t size);
int player_input_profile_set_text(struct player_input_profile *values, const char *name, const char *text);
int player_input_profile_default_text(const char *name, char *text, size_t size);

/* ---------- each local player's values */

/* a local player's values: those of the profile they play with, else the defaults */
const struct player_input_profile *player_input_profile_for_local_player(short local_player_index);
void player_input_profile_set_local_player(short local_player_index, const struct player_input_profile *values);

/* plain-typed accessors for the host's input code (xinput_sdl.c), which declares them itself */
void player_input_profile_mouse(short local_player_index, unsigned short *sensitivity, unsigned short *vertical_sensitivity,
	int *invert, int *aim_assist);
unsigned short player_input_profile_binding(short local_player_index, short action, short slot);

#endif
