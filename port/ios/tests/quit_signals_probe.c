/* Apache-2.0. The Mac apps' quit signals (port/ios/host/host_quit_signals.c): SIGTERM, SIGINT and SIGHUP reach the
   handler on its queue, and the process is not ended by them. */
#include "host_quit_signals.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static dispatch_semaphore_t received;
static volatile int last;

static void handler(int number)
{
	last = number;
	dispatch_semaphore_signal(received);
}

int main(void)
{
	static const int numbers[] = { SIGTERM, SIGINT, SIGHUP };
	dispatch_queue_t queue = dispatch_queue_create("quit-signals-probe", DISPATCH_QUEUE_SERIAL);

	received = dispatch_semaphore_create(0);
	assert(host_quit_signals_install(queue, handler));
	assert(host_quit_signals_install(queue, handler));   /* (a second install keeps the first) */
	usleep(200000);   /* (the sources are registered with the kernel asynchronously) */
	for (unsigned i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++)
	{
		last = 0;
		kill(getpid(), numbers[i]);
		assert(!dispatch_semaphore_wait(received, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)));
		assert(last == numbers[i]);
	}
	puts("quit_signals_probe: ok");
	return 0;
}
