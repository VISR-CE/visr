/*
HOST_FRAME_TIMELINE.C

The open frame timeline row and debug.latency_log (host_frame_timeline.h).
*/

#include "host_frame_timeline.h"
#include "../../runtime/frame_timeline.h"
#include "host_config.h"
#include "host_controller_age.h"
#include "host.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <TargetConditionals.h>
#include <mach/mach_time.h>

#if TARGET_OS_VISION
#define PLATFORM "visionos"
#elif TARGET_OS_TV
#define PLATFORM "tvos"
#elif HALO_NATIVE_MAC || TARGET_OS_MACCATALYST
#define PLATFORM "macos"
#else
#define PLATFORM "ios"
#endif

static struct
{
	pthread_once_t once;
	struct frame_timeline_ring ring;
	struct frame_timeline_row open;
	_Atomic uint64_t frame_id; /* the open row's */
	struct frame_timeline_latency window;
	int log; /* debug.latency_log */
	_Atomic unsigned long handlers, no_time, too_late; /* presented handlers since the last line */
} timeline = { .once = PTHREAD_ONCE_INIT };

static void timeline_start(void)
{
	char value[16];

	frame_timeline_ring_init(&timeline.ring);
	frame_timeline_row_clear(&timeline.open, 1);
	atomic_store(&timeline.frame_id, 1);
	host_config_string("debug.latency_log", "false", value, sizeof(value));
	timeline.log = !strcmp(value, "true");
}

/* CACurrentMediaTime's clock (mach_absolute_time in seconds), which presentedTime uses, without QuartzCore */
static double now_seconds(void)
{
	static mach_timebase_info_data_t base;

	if (!base.denom)
		mach_timebase_info(&base);
	return (double)mach_absolute_time() * (double)base.numer / (double)base.denom / 1e9;
}

static struct frame_timeline_row *open_row(void)
{
	pthread_once(&timeline.once, timeline_start);
	return &timeline.open;
}

void host_frame_timeline_controller_read(void)
{
	struct frame_timeline_row *row = open_row();

	if (isnan(row->controller_read))
	{
		uint64_t age = host_controller_age_ns();

		row->controller_read = now_seconds();
		row->controller_state_age = age == UINT64_MAX ? NAN : (double)age / 1e9;
	}
}

uint64_t host_frame_timeline_frame_id(void)
{
	pthread_once(&timeline.once, timeline_start);
	return atomic_load(&timeline.frame_id);
}

void host_frame_timeline_waits(double display_wait, double encoder_tail)
{
	struct frame_timeline_row *row = open_row();

	if (display_wait >= 0.0)
		row->display_wait = display_wait;
	if (encoder_tail >= 0.0)
		row->encoder_tail = encoder_tail;
}

void host_frame_timeline_barrier(double waited)
{
	struct frame_timeline_row *row = open_row();

	if (waited < 0.0)
		return;
	if (!isnan(row->display_wait))
		waited -= row->display_wait;
	row->encoder_tail = waited > 0.0 ? waited : 0.0;
}

void host_frame_timeline_late_start(double slept)
{
	open_row()->late_start_wait = slept;
}

int host_frame_timeline_logging(void)
{
	pthread_once(&timeline.once, timeline_start);
	return timeline.log;
}

void host_frame_timeline_shown(uint64_t frame_id, double shown)
{
	pthread_once(&timeline.once, timeline_start);
	/* (counted for the line after each window: a presented handler with no presented time, and a shown time for a
	row the ring no longer holds, leave controller_to_shown unknown) */
	atomic_fetch_add(&timeline.handlers, 1);
	if (shown <= 0.0)
		atomic_fetch_add(&timeline.no_time, 1);
	else if (atomic_load(&timeline.frame_id) > frame_id + FRAME_TIMELINE_RING)
		atomic_fetch_add(&timeline.too_late, 1);
	frame_timeline_ring_shown(&timeline.ring, frame_id, shown, FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME);
}

void host_frame_timeline_seal(void)
{
	struct frame_timeline_row *row = open_row(), behind;
	uint64_t id = row->frame_id;

	row->submit = now_seconds();
	frame_timeline_ring_store(&timeline.ring, row);
	frame_timeline_row_clear(row, id + 1);
	atomic_store(&timeline.frame_id, id + 1);
	/* (the window reads the row FRAME_TIMELINE_LAG frames back, whose shown time has had time to arrive) */
	if (timeline.log && id > FRAME_TIMELINE_LAG && frame_timeline_ring_take(&timeline.ring, id - FRAME_TIMELINE_LAG, &behind) &&
		frame_timeline_latency_add(&timeline.window, &behind))
	{
		char line[1024];

		frame_timeline_latency_line(&timeline.window, PLATFORM, line, sizeof(line));
		host_logf(HOST_LOG_INFO, "%s", line);
		host_logf(HOST_LOG_INFO, "latency handlers: %lu presented handlers, %lu with no presented time, %lu after "
			"the row left the ring", atomic_exchange(&timeline.handlers, 0), atomic_exchange(&timeline.no_time, 0),
			atomic_exchange(&timeline.too_late, 0));
	}
}
