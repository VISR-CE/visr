/*
HOST_THREAD_CPU.C

debug.thread_cpu_log (host_thread_cpu.h). At the end of each window the
app's threads are listed (task_threads) and each one's CPU time so far is
compared with what it had at the window's start, keyed by its thread ID, so
threads that start or end within the window count only what they used inside
it. Threads are named as the system names them (pthread_setname_np); a thread
without a name is listed by its ID.
*/

#include "host_thread_cpu.h"
#include "host_config.h"
#include "host_window_stats.h"
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

enum { THREADS = 256, LISTED = 12 };

struct thread_usage
{
	uint64_t id;
	uint64_t microseconds;
	char name[64];
};

static int enabled;
static _Atomic uint64_t main_thread_id;
static unsigned long frames;
static uint64_t window_started;
static struct rusage window_usage;
static struct thread_usage previous[THREADS];
static unsigned previous_count;

static void note_main_thread(void *unused)
{
	uint64_t id = 0;

	(void)unused;
	pthread_threadid_np(NULL, &id);
	atomic_store(&main_thread_id, id);
}

void host_thread_cpu_install(void)
{
	char value[16];

	host_config_string("debug.thread_cpu_log", "false", value, sizeof(value));
	enabled = !strcmp(value, "true");
	if (enabled)
		dispatch_async_f(dispatch_get_main_queue(), NULL, note_main_thread);
}

/* every thread's CPU time so far; returns how many */
static unsigned threads_now(struct thread_usage *usage)
{
	thread_act_array_t list;
	mach_msg_type_number_t count, index;
	unsigned found = 0;

	if (task_threads(mach_task_self(), &list, &count) != KERN_SUCCESS)
		return 0;
	for (index = 0; index < count; index++)
	{
		thread_basic_info_data_t basic;
		thread_identifier_info_data_t identifier;
		thread_extended_info_data_t extended;
		mach_msg_type_number_t size;

		size = THREAD_BASIC_INFO_COUNT;
		if (found < THREADS && thread_info(list[index], THREAD_BASIC_INFO, (thread_info_t)&basic, &size) == KERN_SUCCESS)
		{
			size = THREAD_IDENTIFIER_INFO_COUNT;
			if (thread_info(list[index], THREAD_IDENTIFIER_INFO, (thread_info_t)&identifier, &size) == KERN_SUCCESS)
			{
				struct thread_usage *entry = &usage[found++];

				entry->id = identifier.thread_id;
				entry->microseconds = (uint64_t)(basic.user_time.seconds + basic.system_time.seconds) * 1000000u +
					(uint64_t)(basic.user_time.microseconds + basic.system_time.microseconds);
				entry->name[0] = 0;
				size = THREAD_EXTENDED_INFO_COUNT;
				if (thread_info(list[index], THREAD_EXTENDED_INFO, (thread_info_t)&extended, &size) == KERN_SUCCESS)
					snprintf(entry->name, sizeof(entry->name), "%s", extended.pth_name);
				/* an unnamed thread running a dispatch queue's work: the queue's label */
				if (!entry->name[0] && identifier.dispatch_qaddr)
				{
					dispatch_queue_t queue = *(dispatch_queue_t *)(uintptr_t)identifier.dispatch_qaddr;
					const char *label = queue ? dispatch_queue_get_label(queue) : NULL;

					if (label && label[0])
						snprintf(entry->name, sizeof(entry->name), "queue:%s", label);
				}
			}
		}
		mach_port_deallocate(mach_task_self(), list[index]);
	}
	vm_deallocate(mach_task_self(), (vm_address_t)list, count * sizeof(list[0]));
	return found;
}

struct spent
{
	uint64_t id, microseconds;
	const char *name;
};

static int by_time(const void *a, const void *b)
{
	const struct spent *left = a, *right = b;

	return left->microseconds < right->microseconds ? 1 : left->microseconds > right->microseconds ? -1 : 0;
}

void host_thread_cpu_frame(void)
{
	static struct thread_usage now[THREADS];
	static struct spent spent[THREADS];
	struct rusage usage;
	mach_timebase_info_data_t base;
	unsigned count, index, kept = 0, other;
	uint64_t self = 0, total = 0, main_microseconds = 0, wall_ticks, main_id;
	double per_frame, wall;
	char line[1400];
	int length;

	if (!enabled)
		return;
	wall_ticks = mach_absolute_time();
	main_id = atomic_load(&main_thread_id);
	if (!window_started)
	{
		window_started = wall_ticks;
		getrusage(RUSAGE_SELF, &window_usage);
		previous_count = threads_now(previous);
		return;
	}
	if (++frames < HOST_LOG_WINDOW_FRAMES)
		return;
	count = threads_now(now);
	getrusage(RUSAGE_SELF, &usage);
	pthread_threadid_np(NULL, &self);
	for (index = 0; index < count; index++)
	{
		uint64_t before = 0;

		for (other = 0; other < previous_count; other++)
			if (previous[other].id == now[index].id)
			{
				before = previous[other].microseconds;
				break;
			}
		if (now[index].microseconds <= before)
			continue;
		spent[kept].id = now[index].id;
		spent[kept].microseconds = now[index].microseconds - before;
		/* (a thread seen running a queue's work at the window's start keeps that name if it is idle at its end) */
		if (!now[index].name[0] && other < previous_count)
			memcpy(now[index].name, previous[other].name, sizeof(now[index].name));
		spent[kept].name = now[index].name;
		total += spent[kept].microseconds;
		if (spent[kept].id == main_id)
			main_microseconds = spent[kept].microseconds;
		kept++;
	}
	qsort(spent, kept, sizeof(spent[0]), by_time);
	mach_timebase_info(&base);
	per_frame = 1000.0 * (double)frames;
	wall = (double)(wall_ticks - window_started) * base.numer / base.denom / 1e6 / (double)frames;
	length = snprintf(line, sizeof(line),
		"thread cpu: %lu frames, a frame: wall %.3f ms, app %.3f ms, main %.3f ms%s, context switches %.1f voluntary %.1f involuntary;",
		frames, wall, (double)total / per_frame, (double)main_microseconds / per_frame, main_id ? "" : " (not found)", (double)(usage.ru_nvcsw - window_usage.ru_nvcsw) / (double)frames,
		(double)(usage.ru_nivcsw - window_usage.ru_nivcsw) / (double)frames);
	for (index = 0; index < kept && index < LISTED && length > 0 && (size_t)length < sizeof(line); index++)
	{
		const char *role = spent[index].id == self ? "presenting" : spent[index].id == main_id ? "main" : NULL;
		char label[96];

		if (role)
			snprintf(label, sizeof(label), "%s%s%s", role, spent[index].name[0] ? ":" : "", spent[index].name);
		else if (spent[index].name[0])
			snprintf(label, sizeof(label), "%s", spent[index].name);
		else
			snprintf(label, sizeof(label), "thread-%llu", (unsigned long long)spent[index].id);
		length += snprintf(line + length, sizeof(line) - (size_t)length, " %s %.3f", label,
			(double)spent[index].microseconds / per_frame);
	}
	if (kept > LISTED && length > 0 && (size_t)length < sizeof(line))
	{
		uint64_t rest = 0;

		for (index = LISTED; index < kept; index++)
			rest += spent[index].microseconds;
		snprintf(line + length, sizeof(line) - (size_t)length, " %u others %.3f", kept - LISTED,
			(double)rest / per_frame);
	}
	platform_log("%s", line);
	memcpy(previous, now, count * sizeof(now[0]));
	previous_count = count;
	window_started = wall_ticks;
	window_usage = usage;
	frames = 0;
}
