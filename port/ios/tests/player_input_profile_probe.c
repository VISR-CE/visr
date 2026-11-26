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

/* a payload as a newer build writes it: records of (tag, size, bytes), the container's header and checksum */
static void make_region(unsigned char *region, unsigned version, unsigned reader, const unsigned char *payload, size_t length)
{
	unsigned long crc = crc32_of(payload, length);

	memset(region, 0, PLAYER_INPUT_REGION_SIZE);
	memcpy(region + 14, payload, length);
	memcpy(region, "VINP", 4);
	region[4] = (unsigned char)version;
	region[6] = (unsigned char)reader;
	region[8] = (unsigned char)length; region[9] = (unsigned char)(length >> 8);
	region[10] = (unsigned char)crc; region[11] = (unsigned char)(crc >> 8);
	region[12] = (unsigned char)(crc >> 16); region[13] = (unsigned char)(crc >> 24);
}

static size_t add_record(unsigned char *payload, size_t length, unsigned tag, unsigned size, unsigned seed)
{
	unsigned i;

	payload[length++] = (unsigned char)tag;
	payload[length++] = (unsigned char)size;
	for (i = 0; i < size; i++)
		payload[length++] = (unsigned char)(seed + i * 7);
	return length;
}

/* unknown records of any size a region can hold survive a decode and encode (an older build saving a newer
build's profile), byte for byte, even when they fill the whole payload and our own records have no room left */
static void large_unknown_records_survive(void)
{
	unsigned char payload[PLAYER_INPUT_REGION_SIZE], region[PLAYER_INPUT_REGION_SIZE], again[PLAYER_INPUT_REGION_SIZE];
	struct player_input_extension read, second;
	size_t length = 0;

	length = add_record(payload, length, 0x41, 200, 1);
	length = add_record(payload, length, 0x42, 150, 9);
	payload[length++] = TAG_MOUSE_SENSITIVITY; payload[length++] = 2; payload[length++] = 80; payload[length++] = 0;
	make_region(region, 2, 1, payload, length);
	assert(player_input_extension_decode(region, &read) == _player_input_present);
	assert(read.values.mouse_sensitivity == 80 && read.unknown_length == 354);
	player_input_extension_encode(&read, again);
	assert(player_input_extension_decode(again, &second) == _player_input_present);
	assert(second.unknown_length == 354 && !memcmp(second.unknown, read.unknown, 354) && second.container_version == 2);
	assert(second.values.mouse_sensitivity == 80);
	/* the payload full of records of ours too: nothing unknown is lost (ours that do not fit are dropped) */
	length = 0;
	length = add_record(payload, length, 0x41, 200, 3);
	length = add_record(payload, length, 0x42, 200, 5);
	length = add_record(payload, length, 0x43, 24, 7);
	assert(length == PLAYER_INPUT_REGION_SIZE - 14);
	make_region(region, 5, 1, payload, length);
	assert(player_input_extension_decode(region, &read) == _player_input_present && read.unknown_length == length);
	player_input_extension_encode(&read, again);
	assert(player_input_extension_decode(again, &second) == _player_input_present);
	assert(second.unknown_length == length && !memcmp(second.unknown, read.unknown, length));
}

/* the write thread's block: replaced parts are the new ones, the rest is the file's own, byte for byte */
static void block_merge_keeps_what_it_does_not_replace(void)
{
	unsigned char old_block[512], base[PLAYER_INPUT_REGION_OFFSET], block[512], payload[64], region[PLAYER_INPUT_REGION_SIZE];
	struct player_input_profile input;
	struct player_input_extension read;
	size_t length = 0, i;

	for (i = 0; i < sizeof(old_block); i++)
		old_block[i] = (unsigned char)(i * 31 + 1);
	for (i = 0; i < sizeof(base); i++)
		base[i] = (unsigned char)(200 - i);
	length = add_record(payload, length, 0x41, 9, 4);
	make_region(region, 1, 1, payload, length);
	memcpy(old_block + PLAYER_INPUT_REGION_OFFSET, region, PLAYER_INPUT_REGION_SIZE);
	player_input_profile_defaults(&input);
	input.mouse_sensitivity = 250;
	/* a record write keeps the extension */
	assert(player_input_block_merge(old_block, base, NULL, block));
	assert(!memcmp(block, base, sizeof(base)) && !memcmp(block + 0x44, old_block + 0x44, PLAYER_INPUT_REGION_SIZE));
	/* an extension write keeps the record and checksum as they are (a corrupt one stays corrupt) and a newer build's records */
	assert(player_input_block_merge(old_block, NULL, &input, block));
	assert(!memcmp(block, old_block, 0x44));
	assert(player_input_extension_decode(block + 0x44, &read) == _player_input_present);
	assert(read.values.mouse_sensitivity == 250 && read.unknown_length == 11 && !memcmp(read.unknown, payload, 11));
	/* both */
	assert(player_input_block_merge(old_block, base, &input, block) && !memcmp(block, base, sizeof(base)));
	/* a file that could not be read: only a record write can be built, with the extension absent */
	assert(!player_input_block_merge(NULL, NULL, &input, block));
	assert(player_input_block_merge(NULL, base, NULL, block));
	assert(!memcmp(block, base, sizeof(base)));
	for (i = 0x44; i < 512; i++)
		assert(block[i] == 0);
	assert(player_input_block_merge(NULL, base, &input, block));
	assert(player_input_extension_decode(block + 0x44, &read) == _player_input_present && read.values.mouse_sensitivity == 250);
	/* a record that needs a newer reader is never rewritten */
	make_region(region, 4, 3, payload, length);
	memcpy(old_block + 0x44, region, PLAYER_INPUT_REGION_SIZE);
	assert(player_input_block_merge(old_block, base, &input, block));
	assert(!memcmp(block + 0x44, region, PLAYER_INPUT_REGION_SIZE));
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
	large_unknown_records_survive();
	block_merge_keeps_what_it_does_not_replace();
	out_of_range_values_are_repaired();
	a_newer_reader_requirement_is_reported();
	default_bindings_are_the_ports_old_ones();
	settings_by_name();
	local_players_keep_separate_values();
	return 0;
}
