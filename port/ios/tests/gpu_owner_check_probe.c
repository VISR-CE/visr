/* The owner check (port/ios/host/gpu_owner_check.c): a call from a thread other than its class's owner is counted
and reported once per owner; OTHER_THREAD calls come from any thread; inline (one thread) nothing is reported. Built
and run by tools/test_gpu_owner_check.py with gpu_owner_check.c. */

#include "gpu_owner_check.h"
#include "gpu_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gpu_fake_backend.h"

static unsigned lines;

static void count_line(const char *line)
{
	(void)line;
	lines++;
}

#define CHECK(condition, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "gpu_owner_check_probe: " __VA_ARGS__); \
			fputc('\n', stderr); \
			exit(1); \
		} \
	} while (0)

static const struct gpu_backend *checked;

static void *second_thread(void *argument)
{
	int what = *(const int *)argument;

	if (what == 0)
		checked->flush();
	else if (what == 1)
		(void)checked->texture_create(NULL);
	else
		checked->vertical_blank_wait();
	return NULL;
}

static void on_second_thread(int what)
{
	pthread_t thread;

	CHECK(!pthread_create(&thread, NULL, second_thread, &what), "no second thread");
	pthread_join(thread, NULL);
}

int main(void)
{
	unsigned long before;
	int frame;

	checked = gpu_owner_check_wrap(&fake, count_line, 0);
	/* the owners are the thread that installed the check, even when another thread calls first */
	on_second_thread(0);
	CHECK(gpu_owner_check_violations() == 1 && lines == 1, "a first call from a second thread: %lu violations",
		gpu_owner_check_violations());
	checked = gpu_owner_check_wrap(&fake, count_line, 0);
	lines = 0;
	before = gpu_owner_check_violations();
	/* one thread, as inline: nothing to report */
	for (frame = 0; frame < 3; frame++)
	{
		(void)checked->texture_create(NULL);
		checked->stream_frame_end();
		checked->flush();
		(void)checked->present(0);
	}
	CHECK(gpu_owner_check_violations() == before && lines == 0, "one thread reported %lu violations",
		gpu_owner_check_violations());

	/* a recorded call from a second thread: the consumer's; twice, reported once */
	on_second_thread(0);
	on_second_thread(0);
	CHECK(gpu_owner_check_violations() == before + 2 && lines == 1, "a second consumer: %lu violations, %u lines",
		gpu_owner_check_violations(), lines);
	/* an AT_ONCE call from a second thread: the producer's, reported once more */
	on_second_thread(1);
	CHECK(gpu_owner_check_violations() == before + 3 && lines == 2, "a second producer: %lu violations, %u lines",
		gpu_owner_check_violations(), lines);
	/* an OTHER_THREAD call is not checked */
	before = gpu_owner_check_violations();
	on_second_thread(2);
	CHECK(gpu_owner_check_violations() == before, "vertical_blank_wait from its own thread was reported");
	puts("gpu_owner_check_probe: ok");
	return 0;
}
