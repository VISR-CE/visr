/*
GPU_OWNER_CHECK.C

The backend's calls by the thread their class says (gpu_owner_check.h).
*/

#include "gpu_owner_check.h"
#include "gpu_operation.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

enum { OWNER_CONSUMER, OWNER_PRODUCER, OWNERS };

static const char *const owner_names[OWNERS] = { "the queue's consumer", "the producer" };

static struct
{
	const struct gpu_backend *next;
	struct gpu_backend table;
	void (*report)(const char *line);
	int stop;
	pthread_t owners[OWNERS];
	_Atomic int reported[OWNERS];
	_Atomic unsigned long violations;
} check;

static void owner_check(unsigned owner, unsigned operation)
{
	if (pthread_equal(check.owners[owner], pthread_self()))
		return;
	atomic_fetch_add(&check.violations, 1);
	if (!atomic_exchange(&check.reported[owner], 1) && check.report)
	{
		char line[192];

		snprintf(line, sizeof(line), "encode queue: %s came from a thread other than %s's (reported once)",
			gpu_operation_names[operation], owner_names[owner]);
		check.report(line);
	}
	if (check.stop)
		abort();
}

#define OWNER_OF_RECORDED OWNER_CONSUMER
#define OWNER_OF_AT_ONCE OWNER_PRODUCER
#define OWNER_OF_QUERY OWNER_PRODUCER
#define OWNER_OF_PRODUCER OWNER_PRODUCER
#define CHECKED_RECORDED 1
#define CHECKED_AT_ONCE 1
#define CHECKED_QUERY 1
#define CHECKED_PRODUCER 1
#define CHECKED_OTHER_THREAD 0
#define OWNER_OF_OTHER_THREAD OWNER_PRODUCER
#define CHECKED_THREAD_SAFE 0
#define OWNER_OF_THREAD_SAFE OWNER_PRODUCER

#define OWNED_F(type, name, parameters, arguments) \
	static type owned_##name parameters \
	{ \
		if (GPU_OPERATION_BY_CLASS(CHECKED_, name)) \
			owner_check(GPU_OPERATION_BY_CLASS(OWNER_OF_, name), GPU_OPERATION_##name); \
		return check.next->name arguments; \
	}
#define OWNED_P(name, parameters, arguments) \
	static void owned_##name parameters \
	{ \
		if (GPU_OPERATION_BY_CLASS(CHECKED_, name)) \
			owner_check(GPU_OPERATION_BY_CLASS(OWNER_OF_, name), GPU_OPERATION_##name); \
		check.next->name arguments; \
	}
GPU_BACKEND_OPERATIONS(OWNED_F, OWNED_P)
#undef OWNED_F
#undef OWNED_P

void gpu_owner_check_bind_consumer(void)
{
	check.owners[0] = pthread_self();
	atomic_store(&check.reported[0], 0);
}

const struct gpu_backend *gpu_owner_check_wrap(const struct gpu_backend *chosen, void (*report)(const char *line),
	int stop)
{
	unsigned owner;

	check.next = chosen;
	check.report = report;
	check.stop = stop;
	/* (the thread that chooses the backend, in gpu_initialize, is the game's: inline, it both records and consumes;
	an encode thread rebinds the consumer when it starts) */
	for (owner = 0; owner < OWNERS; owner++)
	{
		check.owners[owner] = pthread_self();
		atomic_store(&check.reported[owner], 0);
	}
	check.table = *chosen;
	/* (a backend without an operation keeps it missing: frame_context is optional) */
#define INSTALL_F(type, name, parameters, arguments) if (chosen->name) check.table.name = owned_##name;
#define INSTALL_P(name, parameters, arguments) if (chosen->name) check.table.name = owned_##name;
	GPU_BACKEND_OPERATIONS(INSTALL_F, INSTALL_P)
#undef INSTALL_F
#undef INSTALL_P
	return &check.table;
}

unsigned long gpu_owner_check_violations(void)
{
	return atomic_load(&check.violations);
}
