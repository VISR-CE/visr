/* frame_timeline.c: the late-join ring keeps a row by frame id and refuses a patch for a row it no longer holds, and
the latency line reports percentiles over known values, counts frames never shown and prints an unknown field as
"-", never as zero */
#include "../../runtime/frame_timeline.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, ...) \
	do \
	{ \
		if (!(condition)) \
		{ \
			fprintf(stderr, "frame_timeline_probe: " __VA_ARGS__); \
			fputc('\n', stderr); \
			exit(1); \
		} \
	} while (0)

int main(void)
{
	static struct frame_timeline_ring ring;
	static struct frame_timeline_latency window;
	struct frame_timeline_row row, taken;
	char line[1024];
	uint64_t id;
	int full = 0;

	frame_timeline_row_clear(&row, 7);
	CHECK(isnan(row.controller_read) && isnan(row.shown) && isnan(row.encoder_tail), "a cleared row has a known field");

	/* the ring: a patch joins its row; one for a row already overwritten is dropped */
	frame_timeline_ring_init(&ring);
	for (id = 1; id <= 20; id++)
	{
		frame_timeline_row_clear(&row, id);
		frame_timeline_ring_store(&ring, &row);
	}
	frame_timeline_ring_shown(&ring, 20, 5.0, FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME);
	CHECK(frame_timeline_ring_take(&ring, 20, &taken) && taken.shown == 5.0, "a shown time did not join its row");
	frame_timeline_ring_shown(&ring, 4, 6.0, FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME);
	CHECK(frame_timeline_ring_take(&ring, 20, &taken) && taken.shown == 5.0, "a late patch for frame 4 overwrote frame 20");
	CHECK(!frame_timeline_ring_take(&ring, 4, &taken), "frame 4 is still in a 16-row ring after frame 20");
	frame_timeline_ring_shown(&ring, 19, 0.0, FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME);
	CHECK(frame_timeline_ring_take(&ring, 19, &taken) && isnan(taken.shown), "a zero presentedTime became a shown time");
	/* a shown time before its row is stored (the encode thread presented it, the game thread had not sealed yet)
	joins the row when it is stored */
	frame_timeline_ring_shown(&ring, 21, 7.0, FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME);
	CHECK(!frame_timeline_ring_take(&ring, 21, &taken), "frame 21 is in the ring before it was stored");
	frame_timeline_row_clear(&row, 21);
	frame_timeline_ring_store(&ring, &row);
	CHECK(frame_timeline_ring_take(&ring, 21, &taken) && taken.shown == 7.0 &&
		taken.shown_basis == FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME, "an early shown time did not join its row");
	frame_timeline_row_clear(&row, 37);
	frame_timeline_ring_store(&ring, &row);
	CHECK(frame_timeline_ring_take(&ring, 37, &taken) && isnan(taken.shown), "frame 21's early shown time joined frame 37");

	/* the window: read at 1.000 s + n ms, shown 30 ms later except every tenth frame; no encoder tail */
	for (id = 1; id <= FRAME_TIMELINE_WINDOW; id++)
	{
		frame_timeline_row_clear(&row, id);
		row.controller_read = 1.0;
		row.submit = 1.0 + 0.010;
		row.controller_state_age = 0.001;
		row.display_wait = 0.002;
		if (id % 10)
		{
			row.shown = 1.0 + 0.020 + (double)(id % 3) * 0.010;
			row.shown_basis = FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME;
		}
		full = frame_timeline_latency_add(&window, &row);
		CHECK(full == (id == FRAME_TIMELINE_WINDOW), "the window filled at frame %llu", (unsigned long long)id);
	}
	frame_timeline_latency_line(&window, "ios", line, sizeof(line));
	CHECK(strstr(line, "latency: frames=300 platform=ios shown_basis=drawable_presented_time ") == line, "%s", line);
	CHECK(strstr(line, " controller_to_submit_p50=10.00 controller_to_submit_p95=10.00"), "%s", line);
	CHECK(strstr(line, " controller_to_shown_p50=30.00 controller_to_shown_p95=40.00"), "%s", line);
	CHECK(strstr(line, " encoder_tail_p50=- encoder_tail_p95=-"), "an unknown field printed as a number: %s", line);
	CHECK(strstr(line, " unshown=30"), "%s", line);
	CHECK(window.frames == 0 && window.counts[FRAME_TIMELINE_METRIC_TO_SHOWN] == 0, "the line did not empty the window");
	puts("frame_timeline_probe: ok");
	return 0;
}
