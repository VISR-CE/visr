/* The player profile's input extension (port/linux/game/player_input_profile.c, included): round trip, old
profiles, damaged and newer records, and each local player's own values. */
#include "../../linux/game/player_input_profile.c"
#include <assert.h>

static void round_trip(void)
{
	struct player_input_extension written, read;
	unsigned char region[PLAYER_INPUT_REGION_SIZE];

	player_input_extension_defaults(&written);
	written.values.mouse_sensitivity = 175;
	written.values.mouse_vertical_sensitivity = 50;
	written.values.invert_mouse = 1;
	written.values.mouse_aim_assist = 1;
	written.values.bindings[6][0] = PLAYER_INPUT_CODE_MOUSE + 5;
	written.values.bindings[11][1] = PLAYER_INPUT_CODE_WHEEL_UP;
	player_input_extension_encode(&written, region);
	assert(player_input_extension_decode(region, &read) == _player_input_present);
	assert(!memcmp(&read.values, &written.values, sizeof(read.values)));
	assert(read.container_version == 1 && read.unknown_length == 0);
}

static void old_and_damaged_profiles_give_defaults(void)
{
	struct player_input_extension read, defaults;
	unsigned char region[PLAYER_INPUT_REGION_SIZE];

	player_input_extension_defaults(&defaults);
	memset(region, 0, sizeof(region));
	assert(player_input_extension_decode(region, &read) == _player_input_absent);
	assert(!memcmp(&read.values, &defaults.values, sizeof(read.values)));
	player_input_extension_encode(&defaults, region);
	region[30] ^= 0x40;
	assert(player_input_extension_decode(region, &read) == _player_input_damaged);
	assert(!memcmp(&read.values, &defaults.values, sizeof(read.values)));
	/* a garbage length cannot read past the region */
	player_input_extension_encode(&defaults, region);
	region[8] = 0xFF;
	region[9] = 0xFF;
	assert(player_input_extension_decode(region, &read) == _player_input_damaged);
}

/* a newer build's record: a version 2 container with an unknown tag, which a save here keeps */
static void newer_records_survive_a_save(void)
{
	struct player_input_extension read, again;
	unsigned char region[PLAYER_INPUT_REGION_SIZE], again_region[PLAYER_INPUT_REGION_SIZE];
	unsigned char *payload = region + 14;
	unsigned long crc;
	size_t length = 0;

	memset(region, 0, sizeof(region));
	payload[length++] = TAG_MOUSE_SENSITIVITY; payload[length++] = 2; payload[length++] = 50; payload[length++] = 0;
	payload[length++] = 0x41; payload[length++] = 3; payload[length++] = 7; payload[length++] = 8; payload[length++] = 9;
	crc = crc32_of(payload, length);
	memcpy(region, "VINP", 4);
	region[4] = 2;
	region[6] = 1;
	region[8] = (unsigned char)length;
	region[10] = (unsigned char)crc; region[11] = (unsigned char)(crc >> 8);
	region[12] = (unsigned char)(crc >> 16); region[13] = (unsigned char)(crc >> 24);
	assert(player_input_extension_decode(region, &read) == _player_input_present);
	assert(read.values.mouse_sensitivity == 50 && read.container_version == 2 && read.unknown_length == 5);
	read.values.invert_mouse = 1;
	player_input_extension_encode(&read, again_region);
	assert(player_input_extension_decode(again_region, &again) == _player_input_present);
	assert(again.container_version == 2 && again.unknown_length == 5 && !memcmp(again.unknown, read.unknown, 5));
	assert(again.values.invert_mouse == 1 && again.values.mouse_sensitivity == 50);
}

/* a record that needs a newer reader is reported as such, not read */
static void a_newer_reader_requirement_is_reported(void)
{
	struct player_input_extension read;
	unsigned char region[PLAYER_INPUT_REGION_SIZE];

	memset(region, 0, sizeof(region));
	memcpy(region, "VINP", 4);
	region[4] = 3;
	region[6] = 3;
	region[10] = 0x00; region[11] = 0; region[12] = 0; region[13] = 0;
	/* (crc of an empty payload is 0) */
	assert(player_input_extension_decode(region, &read) == _player_input_newer);
}

static void out_of_range_values_are_repaired(void)
{
	struct player_input_extension read, bad;
	unsigned char region[PLAYER_INPUT_REGION_SIZE];

	player_input_extension_defaults(&bad);
	bad.values.mouse_sensitivity = 0;
	bad.values.mouse_vertical_sensitivity = 60000;
	bad.values.bindings[0][0] = 0x1234;
	player_input_extension_encode(&bad, region);
	assert(player_input_extension_decode(region, &read) == _player_input_present);
	assert(read.values.mouse_sensitivity == 100 && read.values.mouse_vertical_sensitivity == 0);
	assert(read.values.bindings[0][0] == PLAYER_INPUT_CODE_NONE);
}

/* (the defaults that were config.toml's [controls]: the weapon switches by wheel or Tab, the scores on 1) */
static void default_bindings_are_the_ports_old_ones(void)
{
	struct player_input_profile values;

	player_input_profile_defaults(&values);
	assert(values.bindings[11][0] == PLAYER_INPUT_CODE_WHEEL && values.bindings[11][1] == 43);
	assert(values.bindings[15][0] == 30 && values.bindings[16][0] == 41);
	assert(values.bindings[6][0] == PLAYER_INPUT_CODE_MOUSE + 1 && values.bindings[0][0] == 26);
}

static void settings_by_name(void)
{
	struct player_input_profile values;
	char text[32];

	player_input_profile_defaults(&values);
	assert(player_input_profile_set_text(&values, "profile.mouse_sensitivity", "1.25"));
	assert(values.mouse_sensitivity == 125);
	assert(player_input_profile_text(&values, "profile.mouse_sensitivity", text, sizeof(text)) && !strcmp(text, "1.25"));
	assert(player_input_profile_set_text(&values, "profile.mouse_vertical_sensitivity", "0"));
	assert(player_input_profile_text(&values, "profile.mouse_vertical_sensitivity", text, sizeof(text)) && !strcmp(text, "0"));
	assert(player_input_profile_set_text(&values, "profile.mouse_sensitivity", "2.5"));
	assert(player_input_profile_text(&values, "profile.mouse_sensitivity", text, sizeof(text)) && !strcmp(text, "2.5"));
	assert(player_input_profile_set_text(&values, "profile.invert_mouse", "true") && values.invert_mouse == 1);
	assert(player_input_profile_default_text("profile.mouse_sensitivity", text, sizeof(text)) && !strcmp(text, "1"));
	assert(player_input_profile_default_text("profile.mouse_aim_assist", text, sizeof(text)) && !strcmp(text, "false"));
	assert(!player_input_profile_is_setting("profile.look_sensitivity"));
}

/* the class: each local player reads their own profile's values, and one without a profile reads the defaults */
static void local_players_keep_separate_values(void)
{
	struct player_input_profile one, two;

	player_input_profile_defaults(&one);
	player_input_profile_defaults(&two);
	one.mouse_sensitivity = 300;
	two.mouse_sensitivity = 40;
	two.invert_mouse = 1;
	player_input_profile_set_local_player(0, &one);
	player_input_profile_set_local_player(1, &two);
	assert(player_input_profile_for_local_player(0)->mouse_sensitivity == 300);
	assert(player_input_profile_for_local_player(1)->mouse_sensitivity == 40);
	assert(!player_input_profile_for_local_player(0)->invert_mouse && player_input_profile_for_local_player(1)->invert_mouse);
	assert(player_input_profile_for_local_player(2)->mouse_sensitivity == 100);
	player_input_profile_set_local_player(1, NULL);
	assert(player_input_profile_for_local_player(1)->mouse_sensitivity == 100);
	assert(player_input_profile_for_local_player(0)->mouse_sensitivity == 300);
}

int main(void)
{
	round_trip();
	old_and_damaged_profiles_give_defaults();
	newer_records_survive_a_save();
	out_of_range_values_are_repaired();
	a_newer_reader_requirement_is_reported();
	default_bindings_are_the_ports_old_ones();
	settings_by_name();
	local_players_keep_separate_values();
	return 0;
}
