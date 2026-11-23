/*
The flat frame timeline's waits (port/ios/host/host_frame_timeline.c): with the encode thread, the row's encoder tail
is the present barrier's wait less the display wait inside the present. A frame whose late start slept 5 ms, whose
present waited 4 ms for the display and whose barrier waited 6 ms has a display wait of 4 ms and an encoder tail of
2 ms (the late start is before the controller read, and not in the row's waits at all).
*/

#include "host_frame_timeline.h"
#include "host_controller_age.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, ...) do { if (!(condition)) { fprintf(stderr, "host_frame_timeline_probe: " __VA_ARGS__); \
	fprintf(stderr, "\n"); exit(1); } } while (0)

static char last_line[1024];
static int lines;

void host_config_string(const char *name, const char *fallback, char *out, size_t size)
{
	snprintf(out, size, "%s", !strcmp(name, "debug.latency_log") ? "true" : fallback);
}

uint64_t host_controller_age_ns(void)
{
	return 1000000;
}

void host_logf(int priority, const char *format, ...)
{
	va_list arguments;

	(void)priority;
	char line[1024];

	va_start(arguments, format);
	vsnprintf(line, sizeof(line), format, arguments);
	va_end(arguments);
	/* (the latency line; the handler counts after it are not checked here) */
	if (strncmp(line, "latency: frames=", 16))
		return;
	memcpy(last_line, line, sizeof(line));
	lines++;
}

int main(void)
{
	int frame;

	CHECK(host_frame_timeline_logging(), "debug.latency_log did not turn the line on");
	for (frame = 0; frame < 400 && !lines; frame++)
	{
		host_frame_timeline_late_start(0.005);
		host_frame_timeline_controller_read();
		/* (the backend's present: its display wait; then the dispatch's barrier wait) */
		host_frame_timeline_waits(0.004, -1.0);
		host_frame_timeline_barrier(0.006);
		host_frame_timeline_seal();
	}
	CHECK(lines == 1, "no latency line after %d frames", frame);
	CHECK(strstr(last_line, " display_wait_p50=4.00 display_wait_p95=4.00"), "display wait: %s", last_line);
	CHECK(strstr(last_line, " encoder_tail_p50=2.00 encoder_tail_p95=2.00"), "encoder tail: %s", last_line);
	CHECK(strstr(last_line, " late_start_wait_p50=5.00 late_start_wait_p95=5.00"), "late start: %s", last_line);

	/* without the encode thread there is no barrier: the tail stays unknown (the second window: the first still
	holds the rows the line lags behind) */
	for (lines = 0, frame = 0; frame < 800 && lines < 2; frame++)
	{
		host_frame_timeline_controller_read();
		host_frame_timeline_waits(0.004, -1.0);
		host_frame_timeline_barrier(-1.0);
		host_frame_timeline_seal();
	}
	CHECK(lines == 2 && strstr(last_line, " encoder_tail_p50=- encoder_tail_p95=-"), "tail without a barrier: %s",
		last_line);
	return 0;
}
