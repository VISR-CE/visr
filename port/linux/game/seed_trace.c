/*
SEED_TRACE.C

A debug trace of what a tick consumes, for finding why two runs of one map
diverge (debug.seed_trace; builds made with configure.py --seed-trace and
tools/ios_build.py --seed-trace, compiled out of every other build).

One line per event, in a file in the data folder's recordings folder:

	S <what> <value>                  a seed's origin: the clock-derived local seed, a set global seed
	B <tick>                          a tick begins (the global and local seeds as it found them)
	A <tick> <8 words>                the first local player's action as the tick took it
	G <tick> <caller> <seed>          a draw from the global sequence: the address that asked and the seed it found
	L <tick> <caller> <seed>          the same for the local sequence, inside a tick only
	E <tick> <objects> <units> <actors> <hash> <x,y,z of player 1>
	                                  a tick ends: counts, a hash over every unit's index and position, the seeds

tools/seed_trace_diff.py reads two such files and names the first line that differs.
*/

#ifdef HALO_SEED_TRACE

#include "cseries.h"
#include "ai/actors.h"
#include "game/game.h"
#include "game/players.h"
#include "math/real_math.h"
#include "objects/objects.h"

#include <stdio.h>
#include <string.h>

/* the platform layer's (platform.h, port_config.h) */
void platform_log(const char *format, ...);
const char *platform_data_root(void);
int posix_make_directory(const char *path);
int config_boolean(const char *name);

static FILE *trace_file;
static int trace_state; /* 0 not looked at, 1 on, -1 off */
static int in_tick;
static long current_tick;

/* the settings are read by the first tick (a seed's origin is noted at start-up, before they are): what is noted
before then waits here */
static struct { const char *what; unsigned long value; } pending[16];
static int pending_count;

static int trace_open(void)
{
	char path[512];
	int index;

	if (trace_state != 0)
		return trace_state > 0;
	trace_state = config_boolean("debug.seed_trace") ? 1 : -1;
	platform_log("debug.seed_trace: %s at the first tick", trace_state > 0 ? "on" : "off");
	if (trace_state > 0)
	{
		/* (relative to the data folder, as the recordings are) */
		posix_make_directory("recordings");
		snprintf(path, sizeof(path), "recordings/seed-trace.log");
		trace_file = fopen(path, "w");
		if (!trace_file)
		{
			platform_log("debug.seed_trace: cannot write %s", path);
			trace_state = -1;
		}
		else
		{
			platform_log("debug.seed_trace: writing %s", path);
			for (index = 0; index < pending_count; index++)
				fprintf(trace_file, "S %s %08lx\n", pending[index].what, pending[index].value);
		}
	}
	return trace_state > 0;
}

void seed_trace_note(const char *what, unsigned long value)
{
	if (trace_state > 0)
		fprintf(trace_file, "S %s %08lx\n", what, value);
	else if (trace_state == 0 && pending_count < 16)
	{
		pending[pending_count].what = what;
		pending[pending_count++].value = value;
	}
}

/* get_global_random_seed_address and get_global_local_random_seed_address (random_math.c): the sequences'
draws inside a tick, by the address that asked */
void seed_trace_draw(char which, void *caller, unsigned long seed)
{
	if (!in_tick || trace_state <= 0)
		return;
	fprintf(trace_file, "%c %ld %08lx %08lx\n", which, current_tick, (unsigned long)(size_t)caller, seed);
}

void seed_trace_tick_begin(void)
{
	if (!trace_open())
		return;
	current_tick = game_time_get();
	in_tick = 0; /* (the seeds are read below without being counted as draws) */
	fprintf(trace_file, "B %ld %08lx %08lx\n", current_tick, *get_global_random_seed_address(),
		*get_global_local_random_seed_address());
	in_tick = 1;
}

void seed_trace_action(const void *action, unsigned long size)
{
	const unsigned long *words = (const unsigned long *)action;
	unsigned long index;

	if (!in_tick || trace_state <= 0)
		return;
	fprintf(trace_file, "A %ld", current_tick);
	for (index = 0; index < size / 4 && index < 8; index++)
		fprintf(trace_file, " %08lx", words[index] & 0xffffffffUL);
	fprintf(trace_file, "\n");
}

static unsigned long hash_words(unsigned long hash, unsigned long value)
{
	return (hash ^ value) * 16777619UL & 0xffffffffUL;
}

void seed_trace_tick_end(void)
{
	struct data_iterator iterator;
	struct object_header_datum *header;
	long objects = 0, units = 0, actors = 0;
	unsigned long hash = 2166136261UL;
	float player[3] = { 0, 0, 0 };
	long player_index;

	if (!in_tick || trace_state <= 0)
		return;
	in_tick = 0;
	data_iterator_new(&iterator, object_header_data);
	while ((header = data_iterator_next(&iterator)))
	{
		objects++;
		if (header->datum && (header->type == _object_type_biped || header->type == _object_type_vehicle))
		{
			real_point3d origin;
			union { float f; unsigned long u; } bits;

			units++;
			object_get_origin(iterator.datum_index, &origin);
			hash = hash_words(hash, (unsigned long)iterator.datum_index);
			bits.f = origin.x;
			hash = hash_words(hash, bits.u);
			bits.f = origin.y;
			hash = hash_words(hash, bits.u);
			bits.f = origin.z;
			hash = hash_words(hash, bits.u);
		}
	}
	data_iterator_new(&iterator, actor_data);
	while (data_iterator_next(&iterator))
		actors++;
	player_index = local_player_get_player_index(0);
	if (player_index != NONE)
	{
		struct player_datum *datum = player_try_and_get(player_index);

		if (datum && datum->unit_index != NONE)
		{
			real_point3d origin;

			object_get_origin(datum->unit_index, &origin);
			player[0] = origin.x;
			player[1] = origin.y;
			player[2] = origin.z;
		}
	}
	fprintf(trace_file, "E %ld %ld %ld %ld %08lx %.4f,%.4f,%.4f %08lx %08lx\n", current_tick, objects, units, actors, hash,
		player[0], player[1], player[2], *get_global_random_seed_address(), *get_global_local_random_seed_address());
	fflush(trace_file);
}

#endif
