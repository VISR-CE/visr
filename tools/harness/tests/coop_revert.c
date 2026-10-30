#include "harness.h"
typedef unsigned short word;
enum { MAXIMUM_TRACKED_OBJECTS = 16, MAXIMUM_ENTRIES_PER_MESSAGE = 3, INVENTORY_REFRESH_TICKS = 30,
	_object_type_biped = 0, _object_type_vehicle, _object_type_weapon, _object_dead_bit = 0,
	_object_mask_unit = (1u << _object_type_biped) | (1u << _object_type_vehicle),
	NETWORKED_OBJECT_TYPES = _object_mask_unit | (1u << _object_type_weapon),
	_object_change_delete, _object_change_create, _distributed_message_object_changes,
	_distributed_to_clients_reliably };
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define RELIABLE_ENTRIES(type) 3
#define csmemset memset
struct object_datum { struct { int type; unsigned damage_flags; } object; };
struct object_iterator { int cursor; long index; unsigned mask; };
struct distributed_object_change { int change; long object_index; };
struct distributed_object_change_message { int header; struct distributed_object_change changes[3]; };
static long objects_host_told[MAXIMUM_TRACKED_OBJECTS], objects_host_told_count;
static struct { unsigned long checksum, weapons_checksum; long time, ammunition_time, carried_time; }
	objects_host_inventories[MAXIMUM_TRACKED_OBJECTS];
static struct { int creates, deletes; } objects_statistics;
static struct { long id; boolean networked; struct object_datum datum; } world[MAXIMUM_TRACKED_OBJECTS];
static struct distributed_object_change sent[256];
static int sent_count;
static long game_time_get(void) { return 300; }
static void object_iterator_new(struct object_iterator *it, unsigned mask, byte flags) { it->cursor = 0; it->mask = mask; }
static void *object_iterator_next(struct object_iterator *it)
{
	while (it->cursor < MAXIMUM_TRACKED_OBJECTS)
	{
		int slot = it->cursor++;
		if (world[slot].id != NONE && TEST_FLAG(it->mask, world[slot].datum.object.type)) { it->index = world[slot].id; return &world[slot].datum; }
	}
	return NULL;
}
static struct object_datum *object_get(long id) { return &world[DATUM_INDEX_TO_ABSOLUTE_INDEX(id)].datum; }
static boolean distributed_object_networked(long id) { return world[DATUM_INDEX_TO_ABSOLUTE_INDEX(id)].networked; }
static void distributed_change_from_object(long id, struct distributed_object_change *change)
{ change->object_index = id; change->change = _object_change_create; }
static void distributed_send(void *data, int type, short count, word bytes, int destination)
{
	struct distributed_object_change_message *message = data;
	CHECK(destination == _distributed_to_clients_reliably, "lifecycle must be reliable");
	CHECK(count > 0 && count <= 3, "invalid batch");
	CHECK(bytes == sizeof(message->header) + count * sizeof(message->changes[0]), "invalid length");
	CHECK(sent_count + count <= 256, "too many changes");
	memcpy(sent + sent_count, message->changes, count * sizeof(sent[0])); sent_count += count;
}
#include "under_test.inc"
static void setup(void)
{
	for (int i = 0; i < MAXIMUM_TRACKED_OBJECTS; i++)
	{
		world[i].id = objects_host_told[i] = NONE;
		objects_host_inventories[i].checksum = objects_host_inventories[i].weapons_checksum = 7;
		objects_host_inventories[i].time = 300; objects_host_inventories[i].carried_time = NONE;
	}
	objects_host_told_count = MAXIMUM_TRACKED_OBJECTS;
}
static void add(int slot, int type, boolean dead)
{
	world[slot].id = objects_host_told[slot] = 0x10000 + slot;
	world[slot].networked = TRUE;
	world[slot].datum.object.type = type; world[slot].datum.object.damage_flags = dead;
}
static void fresh(long id)
{
	int deleted = -1, created = -1;
	for (int i = 0; i < sent_count; i++)
		if (sent[i].object_index == id)
		{
			if (sent[i].change == _object_change_delete) deleted = i;
			if (sent[i].change == _object_change_create) created = i;
		}
	CHECK(deleted >= 0 && created > deleted, "restored lifetime requires delete before create");
	CHECK(objects_host_told[DATUM_INDEX_TO_ABSOLUTE_INDEX(id)] == id, "create must update announced ID");
}
int main(int argc, char **argv)
{
	char const *case_name = argv[1]; setup();
	add(0, _object_type_biped, FALSE); add(1, _object_type_biped, FALSE); add(2, _object_type_vehicle, FALSE);
	CASE("unknown") objects_host_told[0] = NONE;
	CASE("changed") objects_host_told[0] = 0x20000;
	CASE("filter")
	{
		add(3, _object_type_weapon, FALSE); add(4, _object_type_biped, TRUE);
		add(5, _object_type_biped, FALSE); world[5].networked = FALSE;
		objects_host_told[5] = NONE;
		objects_host_told[6] = 0x10006; /* absent after rewind: normal diff deletes it */
	}
	CASE("batch") for (int i = 3; i < 16; i++) add(i, _object_type_biped, FALSE);
	network_objects_coop_reverted();
	fresh(world[0].id); fresh(world[1].id); fresh(world[2].id);
	CASE("repeat") { sent_count = 0; network_objects_coop_reverted(); fresh(world[0].id); fresh(world[1].id); }
	CASE("empty")
	{
		CHECK(objects_host_inventories[0].checksum == 0 && objects_host_inventories[0].weapons_checksum == 0,
			"inventory checksums must be invalidated");
		CHECK(game_time_get() - objects_host_inventories[0].time >= INVENTORY_REFRESH_TICKS,
			"force resend even on checksum collision");
		CHECK(objects_host_inventories[0].carried_time == game_time_get(), "empty inventory must also resend");
	}
	CASE("filter")
	{
		CHECK(sent_count == 7, "only living networked units restart; absent object still deleted");
		CHECK(sent[6].object_index == 0x10006 && sent[6].change == _object_change_delete, "normal diff lost");
		CHECK(objects_host_inventories[3].checksum == 7 && objects_host_inventories[4].checksum == 7,
			"unaffected inventory changed");
	}
	CASE("batch") for (int i = 0; i < 16; i++) fresh(world[i].id);
	return 0;
}
