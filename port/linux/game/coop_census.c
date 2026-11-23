/* coop_census.c
port: debug.coop_census, a snapshot of what host and clients must agree on
in a network game (port/linux/NETCODE.md, "Replication contract").

Every debug.coop_census seconds of game time this machine logs one line per
player record, per networked object (bipeds, vehicles, weapons, equipment,
at the indices the host gives them everywhere), per device and per named
object, between a
begin and an end line, all stamped with the tick. Host and clients sample on
the same ticks, so two machines' logs pair up by tick;
tools/coop_census_compare.py compares them under the contract. Each record
is its own line, short enough for the platform log's buffer, so a missing
line means a missing object, not a truncated one. Off (0) costs one compare
a tick. */

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "items/items.h"
#include "items/weapons.h"
#include "devices/devices.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "networking/network_client_manager.h"

#include "coop_census.h"
#include "../src/port_config.h"

#include <string.h>

/* port/linux/src/platform.h (its header brings the host C library, which
clashes with the game's, so game units declare it as it is declared there) */
void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

enum
{
	/* the object types the host keeps at the same indices on every machine
	(network_objects.c's NETWORKED_OBJECT_TYPES) */
	COOP_CENSUS_OBJECT_TYPES = _object_mask_biped | _object_mask_vehicle | _object_mask_weapon | _object_mask_equipment,
};

static struct
{
	boolean read;
	long interval_ticks;
} coop_census;

/* the census interval in ticks, read once (0: off) */
static long coop_census_interval(
	void)
{
	if (!coop_census.read)
	{
		real seconds = (real)config_real("debug.coop_census");

		coop_census.interval_ticks = seconds > 0.0f ? MAX(1L, (long)(seconds * TICKS_PER_SECOND + 0.5f)) : 0;
		coop_census.read = TRUE;
	}
	return coop_census.interval_ticks;
}

/* where the player sits in the machine table: machine and local slot, or
NONE for both when no machine has it */
static void coop_census_seat(
	long player_index,
	long *machine,
	long *slot)
{
	long machine_index;

	*machine = NONE;
	*slot = NONE;
	for (machine_index = 0; machine_index < HALO_PORT_MAXIMUM_NETWORK_MACHINES; machine_index++)
	{
		long *players = machine_get_player_list(machine_index);
		long local_index;

		if (!players)
			continue;
		for (local_index = 0; local_index < MAXIMUM_LOCAL_PLAYERS; local_index++)
		{
			if (players[local_index] != NONE &&
				DATUM_INDEX_TO_ABSOLUTE_INDEX(players[local_index]) == DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index))
			{
				*machine = machine_index;
				*slot = local_index;
				return;
			}
		}
	}
}

/* player records, by absolute index: clients number their own identifiers,
so only the absolute index is shared */
static void coop_census_players(
	long tick)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long machine, slot;

		coop_census_seat(iterator.datum_index, &machine, &slot);
		platform_log("coop census: tick %ld player %ld id %08lx machine %d seat %ld/%ld team %ld quit %d unit %08lx",
			tick, (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index), (unsigned long)iterator.datum_index,
			(int)player->network_player_data.machine_index, machine, slot, (long)player->team_index,
			(int)player->quit_out_of_game, (unsigned long)player->unit_index);
	}
}

/* one networked object, and what a unit carries or a weapon holds */
static void coop_census_object(
	long tick,
	long object_index)
{
	struct object_datum *object = object_get(object_index);

	platform_log("coop census: tick %ld object %08lx type %d def %08lx parent %08lx dead %d map %d pos %.3f %.3f %.3f",
		tick, (unsigned long)object_index, (int)object->object.type, (unsigned long)object->definition_index,
		(unsigned long)object->object.parent_object_index,
		(int)TEST_FLAG(object->object.damage_flags, _object_dead_bit),
		(int)TEST_FLAG(object->object.flags, _object_connected_to_map_bit),
		object->object.position.x, object->object.position.y, object->object.position.z);
	if (TEST_FLAG(_object_mask_unit, object->object.type))
	{
		struct unit_datum *unit = unit_get(object_index);

		platform_log("coop census: tick %ld unit %08lx player %ld seat %d active %d grenades %d %d current %d weapons %08lx %08lx %08lx %08lx",
			tick, (unsigned long)object_index,
			unit->unit.player_index == NONE ? -1L : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.player_index),
			(int)unit->unit.parent_seat_index,
			(int)TEST_FLAG(unit->unit.flags, _unit_actively_controlled_bit),
			(int)unit->unit.grenade_counts[0], (int)unit->unit.grenade_counts[1], (int)unit->unit.current_weapon_index,
			(unsigned long)unit->unit.weapon_object_indices[0], (unsigned long)unit->unit.weapon_object_indices[1],
			(unsigned long)unit->unit.weapon_object_indices[2], (unsigned long)unit->unit.weapon_object_indices[3]);
	}
	else if (object->object.type == _object_type_weapon)
	{
		struct weapon_datum *weapon = weapon_get(object_index);

		platform_log("coop census: tick %ld weapon %08lx carried %d rounds %d %d %d %d age %.3f",
			tick, (unsigned long)object_index, (int)TEST_FLAG(weapon->item.flags, _item_attached_to_unit_bit),
			(int)weapon->weapon.magazines[0].rounds_total, (int)weapon->weapon.magazines[0].rounds_loaded,
			(int)weapon->weapon.magazines[1].rounds_total, (int)weapon->weapon.magazines[1].rounds_loaded,
			weapon->weapon.age);
	}
}

static void coop_census_log(
	long tick)
{
	struct object_iterator iterator;
	long objects = 0, devices = 0;
	boolean host = game_connection() == _game_connection_network_server;
	struct scenario *scenario = global_scenario_get();

	platform_log("coop census: tick %ld begin %s version %u", tick, host ? "host" : "client", network_game_host_version());
	coop_census_players(tick);
	object_iterator_new(&iterator, COOP_CENSUS_OBJECT_TYPES, 0);
	while (object_iterator_next(&iterator))
	{
		/* (a client's own objects, effects and the like, live in the upper
		half and are its alone) */
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.index) >= MAXIMUM_OBJECTS_PER_MAP / 2)
			continue;
		coop_census_object(tick, iterator.index);
		objects++;
	}
	object_iterator_new(&iterator, _object_mask_device, 0);
	while (object_iterator_next(&iterator))
	{
		struct device_datum *device = device_get(iterator.index);
		char const *tag = tag_get_name(device->definition_index);
		char const *leaf = tag ? strrchr(tag, '\\') : NULL;

		/* (the groups and tag name tell a scripted device from one that moves on its own) */
		platform_log("coop census: tick %ld device %08lx position %.3f power %.3f groups %d %d tag %.40s",
			tick, (unsigned long)iterator.index, device_get_position(iterator.index), device_get_power(iterator.index),
			(int)device->device.position_group_index, (int)device->device.power_group_index,
			leaf ? leaf + 1 : tag ? tag : "?");
		devices++;
	}
	/* (the scenario's names of the objects there, for tests that name one) */
	if (scenario)
	{
		short name_index;

		for (name_index = 0; name_index < scenario->object_names.count; name_index++)
		{
			struct scenario_object_name *name = TAG_BLOCK_GET_ELEMENT(&scenario->object_names,
				name_index, struct scenario_object_name);
			long object_index = object_index_from_name_index(name_index);

			if (object_index != NONE)
				platform_log("coop census: tick %ld name %08lx %.32s", tick, (unsigned long)object_index, name->name);
		}
	}
	platform_log("coop census: tick %ld end objects %ld devices %ld", tick, objects, devices);
}

void coop_census_tick(
	void)
{
	long interval = coop_census_interval();
	long tick;

	if (!interval || game_connection() == _game_connection_local || !game_in_progress())
		return;
	tick = game_time_get();
	if (tick > 0 && tick % interval == 0)
		coop_census_log(tick);
}
