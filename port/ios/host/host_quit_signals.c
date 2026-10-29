/* Apache-2.0. The Mac apps quit on SIGTERM, SIGINT and SIGHUP (a logout, `kill`, a test harness) the way they do on
   Command-Q. SDL turns SIGINT and SIGTERM into a quit event, but only when something polls its events, which the import
   screens and the AppKit app's modal windows do not: there such a signal was never acted on, and a harness's `kill`
   left the app running. The default action is turned off and a dispatch source per signal delivers it to a handler on
   a queue instead. SDL leaves a signal alone whose handler is not the default, whichever is set first. */
#include "host_quit_signals.h"
#include <signal.h>

int host_quit_signals_install(dispatch_queue_t queue, host_quit_signal_handler handler)
{
	static const int numbers[] = { SIGTERM, SIGINT, SIGHUP };
	static dispatch_source_t sources[sizeof(numbers) / sizeof(numbers[0])];
	int watched = 0;

	for (unsigned i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++)
	{
		int number = numbers[i];
		dispatch_source_t source;

		if (sources[i])
		{
			watched++;
			continue;
		}
		source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)number, 0, queue);
		if (!source)
			continue;
		/* (ignored as a signal, so the source sees it and the process is not ended before the handler runs) */
		signal(number, SIG_IGN);
		dispatch_source_set_event_handler(source, ^{ handler(number); });
		dispatch_resume(source);
		sources[i] = source;
		watched++;
	}
	return watched == (int)(sizeof(numbers) / sizeof(numbers[0]));
}
