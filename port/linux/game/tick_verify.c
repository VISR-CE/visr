/*
TICK_VERIFY.C

debug.verify_once_a_tick. A block cache (lruv_cache.c) and a memory pool
(memory_pool.c) walk their whole block list to verify it at every allocation
and free. A crowded game's tick makes and frees hundreds of decals, each
walking a list hundreds long. With the
setting, a walk asked for inside a game tick or a game frame is put off to
the end of that tick or frame, where each cache that asked is walked once: a
list gone wrong still stops the game there, by the same assertions, before
anything is drawn or the next tick starts. A block leaving the list is
checked as it leaves (lruv_block_delete, memory_pool_block_get), since the
walk at the end would no longer find it; a pool is walked before it is
compacted, which writes every link again; and a cache or pool going away is
walked first. Outside a tick and a
frame (loading, rendering) every walk happens where it is asked for, as
before. The checks that are not walks (signatures, the block being used) are
not put off.

The walks read and write nothing of the game: the setting changes how much
work a tick does and not what it does. The caches and pools are the game
thread's alone, and so is what is noted here.
*/

#include "cseries.h"

/* sim_experiments.c's (debug.verify_once_a_tick) */
boolean sim_experiment_enabled(short experiment);
#define SIM_EXPERIMENT_VERIFY_ONCE_A_TICK 4

enum { MAXIMUM_TICK_VERIFY_SUBJECTS = 16 };

static struct
{
	void *subject;
	void (*verify)(void *subject);
} tick_verify_subjects[MAXIMUM_TICK_VERIFY_SUBJECTS];
static short tick_verify_subject_count;
static boolean tick_verify_inside; /* inside a tick or a frame */
static boolean tick_verify_putting_off;

/* the tick or frame ends: every walk put off happens now */
void tick_verify_end(
	void)
{
	short index;

	/* (no longer inside: a walk that asks again is not put off) */
	tick_verify_inside = FALSE;
	for (index = 0; index < tick_verify_subject_count; index++)
	{
		tick_verify_subjects[index].verify(tick_verify_subjects[index].subject);
	}
	tick_verify_subject_count = 0;
}

/* a game tick or game frame begins. They do not nest: one begun inside another means an end was missed, and the
walks owed are made here rather than never. */
void tick_verify_begin(
	void)
{
	if (tick_verify_inside)
	{
		tick_verify_end();
	}
	tick_verify_inside = TRUE;
	tick_verify_putting_off = sim_experiment_enabled(SIM_EXPERIMENT_VERIFY_ONCE_A_TICK);
}

/* whether the walk of a cache is put off to the end of the tick or frame, where verify is called with it; FALSE
has the caller walk now (no tick or frame, the setting off, or no room to note it) */
boolean tick_verify_put_off(
	void *subject,
	void (*verify)(void *subject))
{
	short index;

	if (!tick_verify_inside || !tick_verify_putting_off)
	{
		return FALSE;
	}
	for (index = 0; index < tick_verify_subject_count; index++)
	{
		if (tick_verify_subjects[index].subject == subject)
		{
			return TRUE;
		}
	}
	if (tick_verify_subject_count >= MAXIMUM_TICK_VERIFY_SUBJECTS)
	{
		return FALSE;
	}
	tick_verify_subjects[tick_verify_subject_count].subject = subject;
	tick_verify_subjects[tick_verify_subject_count].verify = verify;
	tick_verify_subject_count++;

	return TRUE;
}

/* a cache is going away: a walk of it put off happens now */
void tick_verify_flush(
	void *subject)
{
	short index;

	for (index = 0; index < tick_verify_subject_count; index++)
	{
		if (tick_verify_subjects[index].subject == subject)
		{
			void (*verify)(void *subject) = tick_verify_subjects[index].verify;
			boolean const putting_off = tick_verify_putting_off;

			tick_verify_subjects[index] = tick_verify_subjects[--tick_verify_subject_count];
			tick_verify_putting_off = FALSE;
			verify(subject);
			tick_verify_putting_off = putting_off;
			break;
		}
	}
}
