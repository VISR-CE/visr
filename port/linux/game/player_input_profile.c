/*
PLAYER_INPUT_PROFILE.C

The input extension of a player profile file.

The Xbox profile file is one 512-byte block: the 0x30-byte profile record, a
20-byte checksum of it, and 452 bytes of padding that no build ever read. The
extension sits in that padding, from offset 0x44:

	"VINP"   4 bytes   magic
	version  u16 LE    the writer's container version (this build writes 1)
	reader   u16 LE    the least version that may interpret the payload; a build
	                   older than that leaves the whole region alone
	length   u16 LE    payload bytes
	crc32    u32 LE    of the payload
	payload  length    records of (tag u8, size u8, value)

Records only ever get added: a tag's meaning and size never change, a build
skips the tags it does not know, and keeps them (unknown[]) so that writing a
profile back loses none of a newer build's settings. A profile with no magic
(every profile an older build wrote, whose padding is zeros), a bad checksum or
a payload that does not parse gives the defaults and never touches the base
record. Builds from before the extension read only the record and its
checksum, so they ignore it; one that saves the profile writes zeros over it,
and the input settings are the defaults again.

Tags: 0x01 mouse sensitivity (u16, hundredths), 0x02 vertical (u16),
0x03 invert (u8), 0x04 aim assist (u8), 0x10 the bindings (17 actions of 2
u16 codes). 0x40 to 0x5F are the touch controls'.
*/

#include "player_input_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC_0 'V'
#define MAGIC_1 'I'
#define MAGIC_2 'N'
#define MAGIC_3 'P'
#define CONTAINER_VERSION 1
#define MINIMUM_READER_VERSION 1
#define HEADER_SIZE 14

#define TAG_MOUSE_SENSITIVITY 0x01
#define TAG_MOUSE_VERTICAL 0x02
#define TAG_INVERT_MOUSE 0x03
#define TAG_MOUSE_AIM_ASSIST 0x04
#define TAG_BINDINGS 0x10

#define MAXIMUM_SENSITIVITY 1000
#define BINDINGS_SIZE (PLAYER_INPUT_BINDING_ACTIONS * PLAYER_INPUT_BINDING_SLOTS * 2)

/* the bindings every profile starts with: the codes of the SDL scancodes
(USB HID usages: A is 4 to Z 29, 1 is 30, Escape 41, Space 44, Tab 43, Left
Ctrl 224) and the mouse's; the order is enum halo_keyboard_action's */
#define KEY_A 4
#define KEY_C 6
#define KEY_D 7
#define KEY_E 8
#define KEY_F 9
#define KEY_G 10
#define KEY_Q 20
#define KEY_R 21
#define KEY_S 22
#define KEY_W 26
#define KEY_X 27
#define KEY_Z 29
#define KEY_1 30
#define KEY_ESCAPE 41
#define KEY_TAB 43
#define KEY_SPACE 44
#define KEY_LEFT_CONTROL 224
#define MOUSE_LEFT (PLAYER_INPUT_CODE_MOUSE + 1)
#define MOUSE_MIDDLE (PLAYER_INPUT_CODE_MOUSE + 2)
#define MOUSE_RIGHT (PLAYER_INPUT_CODE_MOUSE + 3)
#define MOUSE_4 (PLAYER_INPUT_CODE_MOUSE + 4)

static const unsigned short default_bindings[PLAYER_INPUT_BINDING_ACTIONS][PLAYER_INPUT_BINDING_SLOTS] =
{
	{ KEY_W, 0 },                   /* move forward */
	{ KEY_S, 0 },                   /* move backward */
	{ KEY_A, 0 },                   /* strafe left */
	{ KEY_D, 0 },                   /* strafe right */
	{ KEY_SPACE, 0 },               /* jump */
	{ KEY_LEFT_CONTROL, KEY_C },    /* crouch */
	{ MOUSE_LEFT, 0 },              /* fire */
	{ MOUSE_RIGHT, KEY_G },         /* throw grenade */
	{ KEY_F, MOUSE_4 },             /* melee */
	{ KEY_R, 0 },                   /* reload */
	{ KEY_Z, MOUSE_MIDDLE },        /* zoom */
	{ PLAYER_INPUT_CODE_WHEEL, KEY_TAB }, /* switch weapon */
	{ KEY_X, 0 },                   /* switch grenade */
	{ KEY_E, 0 },                   /* action */
	{ KEY_Q, 0 },                   /* flashlight */
	{ KEY_1, 0 },                   /* scoreboard */
	{ KEY_ESCAPE, 0 },              /* pause */
};

static unsigned long crc32_of(const unsigned char *data, size_t size)
{
	unsigned long crc = 0xFFFFFFFFul;
	size_t index;
	int bit;

	for (index = 0; index < size; index++)
	{
		crc ^= data[index];
		for (bit = 0; bit < 8; bit++)
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320ul : crc >> 1;
	}
	return crc ^ 0xFFFFFFFFul;
}

static unsigned short read_u16(const unsigned char *bytes)
{
	return (unsigned short)(bytes[0] | (bytes[1] << 8));
}

static void write_u16(unsigned char *bytes, unsigned short value)
{
	bytes[0] = (unsigned char)(value & 0xFF);
	bytes[1] = (unsigned char)(value >> 8);
}

/* ---------- values */

void player_input_profile_defaults(struct player_input_profile *values)
{
	memset(values, 0, sizeof(*values));
	values->mouse_sensitivity = 100;
	values->mouse_vertical_sensitivity = 0;
	values->invert_mouse = 0;
	values->mouse_aim_assist = 0;
	memcpy(values->bindings, default_bindings, sizeof(values->bindings));
}

void player_input_extension_defaults(struct player_input_extension *extension)
{
	memset(extension, 0, sizeof(*extension));
	player_input_profile_defaults(&extension->values);
	extension->container_version = CONTAINER_VERSION;
}

static unsigned short clamp_sensitivity(unsigned short value, unsigned short minimum, unsigned short fallback)
{
	if (value < minimum || value > MAXIMUM_SENSITIVITY)
		return fallback;
	return value;
}

/* a binding that is a code this build knows how to name, else none */
static unsigned short valid_binding(unsigned short code)
{
	if (code < PLAYER_INPUT_CODE_MOUSE + 16 || (code >= PLAYER_INPUT_CODE_WHEEL && code <= PLAYER_INPUT_CODE_WHEEL_DOWN))
		return code;
	return PLAYER_INPUT_CODE_NONE;
}

/* ---------- the codec */

enum player_input_decode player_input_extension_decode(const unsigned char *region, struct player_input_extension *extension)
{
	unsigned short version, reader, length;
	unsigned long crc;
	const unsigned char *payload, *end;

	player_input_extension_defaults(extension);
	if (region[0] != MAGIC_0 || region[1] != MAGIC_1 || region[2] != MAGIC_2 || region[3] != MAGIC_3)
		return _player_input_absent;
	version = read_u16(region + 4);
	reader = read_u16(region + 6);
	length = read_u16(region + 8);
	crc = (unsigned long)region[10] | ((unsigned long)region[11] << 8) | ((unsigned long)region[12] << 16) |
		((unsigned long)region[13] << 24);
	if (version < 1 || length > PLAYER_INPUT_REGION_SIZE - HEADER_SIZE)
		return _player_input_damaged;
	payload = region + HEADER_SIZE;
	if (crc32_of(payload, length) != crc)
		return _player_input_damaged;
	extension->container_version = version;
	if (reader > CONTAINER_VERSION)
		return _player_input_newer;
	for (end = payload + length; payload + 2 <= end;)
	{
		unsigned char tag = payload[0], size = payload[1];
		const unsigned char *value = payload + 2;

		if (value + size > end)
		{
			/* a record that runs past the payload: take what was read before it */
			break;
		}
		if (tag == TAG_MOUSE_SENSITIVITY && size == 2)
			extension->values.mouse_sensitivity = clamp_sensitivity(read_u16(value), 5, 100);
		else if (tag == TAG_MOUSE_VERTICAL && size == 2)
			extension->values.mouse_vertical_sensitivity = clamp_sensitivity(read_u16(value), 0, 0);
		else if (tag == TAG_INVERT_MOUSE && size == 1)
			extension->values.invert_mouse = value[0] != 0;
		else if (tag == TAG_MOUSE_AIM_ASSIST && size == 1)
			extension->values.mouse_aim_assist = value[0] != 0;
		else if (tag == TAG_BINDINGS && size == BINDINGS_SIZE)
		{
			int action, slot;

			for (action = 0; action < PLAYER_INPUT_BINDING_ACTIONS; action++)
				for (slot = 0; slot < PLAYER_INPUT_BINDING_SLOTS; slot++)
					extension->values.bindings[action][slot] =
						valid_binding(read_u16(value + (action * PLAYER_INPUT_BINDING_SLOTS + slot) * 2));
		}
		else if (extension->unknown_length + 2u + size <= PLAYER_INPUT_UNKNOWN_SIZE)
		{
			memcpy(extension->unknown + extension->unknown_length, payload, 2u + size);
			extension->unknown_length = (unsigned short)(extension->unknown_length + 2u + size);
		}
		payload = value + size;
	}
	return _player_input_present;
}

void player_input_extension_encode(const struct player_input_extension *extension, unsigned char *region)
{
	unsigned char *payload = region + HEADER_SIZE, *at = payload;
	const struct player_input_profile *values = &extension->values;
	unsigned long crc;
	int action, slot;

	memset(region, 0, PLAYER_INPUT_REGION_SIZE);
	*at++ = TAG_MOUSE_SENSITIVITY;
	*at++ = 2;
	write_u16(at, values->mouse_sensitivity);
	at += 2;
	*at++ = TAG_MOUSE_VERTICAL;
	*at++ = 2;
	write_u16(at, values->mouse_vertical_sensitivity);
	at += 2;
	*at++ = TAG_INVERT_MOUSE;
	*at++ = 1;
	*at++ = values->invert_mouse != 0;
	*at++ = TAG_MOUSE_AIM_ASSIST;
	*at++ = 1;
	*at++ = values->mouse_aim_assist != 0;
	*at++ = TAG_BINDINGS;
	*at++ = BINDINGS_SIZE;
	for (action = 0; action < PLAYER_INPUT_BINDING_ACTIONS; action++)
	{
		for (slot = 0; slot < PLAYER_INPUT_BINDING_SLOTS; slot++)
		{
			write_u16(at, values->bindings[action][slot]);
			at += 2;
		}
	}
	memcpy(at, extension->unknown, extension->unknown_length);
	at += extension->unknown_length;
	crc = crc32_of(payload, (size_t)(at - payload));
	region[0] = MAGIC_0;
	region[1] = MAGIC_1;
	region[2] = MAGIC_2;
	region[3] = MAGIC_3;
	write_u16(region + 4, extension->container_version > CONTAINER_VERSION ? extension->container_version :
		CONTAINER_VERSION);
	/* (a record whose reader version is past ours is never rewritten: see _player_input_newer) */
	write_u16(region + 6, MINIMUM_READER_VERSION);
	write_u16(region + 8, (unsigned short)(at - payload));
	region[10] = (unsigned char)(crc & 0xFF);
	region[11] = (unsigned char)((crc >> 8) & 0xFF);
	region[12] = (unsigned char)((crc >> 16) & 0xFF);
	region[13] = (unsigned char)((crc >> 24) & 0xFF);
}

/* ---------- settings by name */

static const struct
{
	const char *name;
	/* 0 hundredths, 1 boolean */
	int boolean;
	size_t offset;
	size_t size;
} fields[] =
{
	{ "profile.mouse_sensitivity", 0, offsetof(struct player_input_profile, mouse_sensitivity), 2 },
	{ "profile.mouse_vertical_sensitivity", 0, offsetof(struct player_input_profile, mouse_vertical_sensitivity), 2 },
	{ "profile.invert_mouse", 1, offsetof(struct player_input_profile, invert_mouse), 1 },
	{ "profile.mouse_aim_assist", 1, offsetof(struct player_input_profile, mouse_aim_assist), 1 },
};

static int field_of(const char *name)
{
	size_t index;

	for (index = 0; index < sizeof(fields) / sizeof(fields[0]); index++)
	{
		if (!strcmp(name, fields[index].name))
			return (int)index;
	}
	return -1;
}

int player_input_profile_is_setting(const char *name)
{
	return field_of(name) >= 0;
}

static int field_text(const struct player_input_profile *values, int field, char *text, size_t size)
{
	const unsigned char *at = (const unsigned char *)values + fields[field].offset;
	unsigned value = fields[field].size == 2 ? *(const unsigned short *)at : *at;

	if (fields[field].boolean)
		snprintf(text, size, "%s", value ? "true" : "false");
	else if (value % 100 == 0)
		snprintf(text, size, "%u", value / 100);
	else if (value % 10 == 0)
		snprintf(text, size, "%u.%u", value / 100, (value % 100) / 10);
	else
		snprintf(text, size, "%u.%02u", value / 100, value % 100);
	return 1;
}

int player_input_profile_text(const struct player_input_profile *values, const char *name, char *text, size_t size)
{
	int field = field_of(name);

	return field >= 0 && field_text(values, field, text, size);
}

int player_input_profile_default_text(const char *name, char *text, size_t size)
{
	struct player_input_profile values;
	int field = field_of(name);

	player_input_profile_defaults(&values);
	return field >= 0 && field_text(&values, field, text, size);
}

int player_input_profile_set_text(struct player_input_profile *values, const char *name, const char *text)
{
	int field = field_of(name);
	unsigned char *at = (unsigned char *)values + (field >= 0 ? fields[field].offset : 0);

	if (field < 0)
		return 0;
	if (fields[field].boolean)
	{
		*at = !strcmp(text, "true") || !strcmp(text, "1");
		return 1;
	}
	{
		double number = atof(text);
		unsigned value = number <= 0.0 ? 0 : (unsigned)(number * 100.0 + 0.5);
		unsigned short minimum = !strcmp(name, "profile.mouse_sensitivity") ? 5 : 0;
		unsigned short fallback = minimum ? 100 : 0;
		unsigned short stored = value > MAXIMUM_SENSITIVITY ? 0xFFFF : (unsigned short)value;

		stored = clamp_sensitivity(stored, minimum, fallback);
		memcpy(at, &stored, sizeof(stored));
	}
	return 1;
}

/* ---------- each local player's values */

static struct player_input_profile local_values[PLAYER_INPUT_LOCAL_PLAYERS];
static int local_values_ready;

static void local_values_prepare(void)
{
	int index;

	if (local_values_ready)
		return;
	for (index = 0; index < PLAYER_INPUT_LOCAL_PLAYERS; index++)
		player_input_profile_defaults(&local_values[index]);
	local_values_ready = 1;
}

const struct player_input_profile *player_input_profile_for_local_player(short local_player_index)
{
	local_values_prepare();
	if (local_player_index < 0 || local_player_index >= PLAYER_INPUT_LOCAL_PLAYERS)
		local_player_index = 0;
	return &local_values[local_player_index];
}

void player_input_profile_set_local_player(short local_player_index, const struct player_input_profile *values)
{
	local_values_prepare();
	if (local_player_index < 0 || local_player_index >= PLAYER_INPUT_LOCAL_PLAYERS)
		return;
	if (values)
		local_values[local_player_index] = *values;
	else
		player_input_profile_defaults(&local_values[local_player_index]);
}

void player_input_profile_mouse(short local_player_index, unsigned short *sensitivity, unsigned short *vertical_sensitivity,
	int *invert, int *aim_assist)
{
	const struct player_input_profile *values = player_input_profile_for_local_player(local_player_index);

	*sensitivity = values->mouse_sensitivity;
	*vertical_sensitivity = values->mouse_vertical_sensitivity;
	*invert = values->invert_mouse;
	*aim_assist = values->mouse_aim_assist;
}

unsigned short player_input_profile_binding(short local_player_index, short action, short slot)
{
	if (action < 0 || action >= PLAYER_INPUT_BINDING_ACTIONS || slot < 0 || slot >= PLAYER_INPUT_BINDING_SLOTS)
		return PLAYER_INPUT_CODE_NONE;
	return player_input_profile_for_local_player(local_player_index)->bindings[action][slot];
}
