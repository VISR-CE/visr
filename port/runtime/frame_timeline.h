/*
FRAME_TIMELINE.H

One row per presented frame, on one clock (CACurrentMediaTime's), and the
reductions every platform prints from the rows. Plain C with no platform calls,
so the app and the probes run the same code. The design is the frame timeline
spec (2026-11-21, stereo benchmark timeline), whose names this uses; this file
holds the fields the flat Metal present has so far, and the debug.latency_log
line (its section 6).

A field a platform or a frame does not have is NAN in the row and "-" in a
line; it is never written as zero.

Values that arrive after the frame (a drawable's presentedTime) join the row
by frame id in a ring of FRAME_TIMELINE_RING rows; the latency window reads
rows FRAME_TIMELINE_LAG frames behind the newest, so late values have arrived.
*/

#ifndef __HALO_FRAME_TIMELINE_H
#define __HALO_FRAME_TIMELINE_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#define FRAME_TIMELINE_RING 16
#define FRAME_TIMELINE_LAG 8
#define FRAME_TIMELINE_WINDOW 300

enum frame_timeline_shown_basis
{
	FRAME_TIMELINE_SHOWN_NONE,
	FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME,
	FRAME_TIMELINE_SHOWN_COMPOSITOR_PREDICTED,
};

struct frame_timeline_row
{
	uint64_t frame_id;  /* 1 and up; 0 is no row */
	unsigned shown_basis;
	/* stamps (seconds) */
	double controller_read; /* the game's controller read, after halo_frame_start returned */
	double submit;          /* the frame's last CPU act: presentDrawable and the commit done (flat) */
	double shown;           /* when the display showed it (presentedTime) */
	/* durations (seconds) */
	double controller_state_age; /* how old the driver's state was at the read */
	double display_wait;         /* blocked for the display or a frame slot inside the frame */
	double encoder_tail;         /* with the encode thread: the game thread's wait at the present barrier that was
	                                the encoder's work */
	double late_start_wait;      /* the pacer's sleep before controller_read (display.late_start) */
};

/* a row with every field unknown */
void frame_timeline_row_clear(struct frame_timeline_row *row, uint64_t frame_id);

/* the late-join ring: stored by the thread that seals rows, patched by completion handlers on any thread */
struct frame_timeline_ring
{
	pthread_mutex_t lock;
	struct frame_timeline_row rows[FRAME_TIMELINE_RING];
	/* a shown time that came before its row was stored (with the encode thread the display can show a frame before
	the game thread, woken from the present barrier, seals its row): kept by slot, joined at the store */
	struct
	{
		uint64_t frame_id;
		double shown;
		unsigned basis;
	} early[FRAME_TIMELINE_RING];
};

void frame_timeline_ring_init(struct frame_timeline_ring *ring);
void frame_timeline_ring_store(struct frame_timeline_ring *ring, const struct frame_timeline_row *row);
/* the display showed frame_id at shown (0 or less: it never was); a row already gone from the ring is left alone,
and one not stored yet gets it when it is */
void frame_timeline_ring_shown(struct frame_timeline_ring *ring, uint64_t frame_id, double shown, unsigned basis);
/* copies frame_id's row out; 0 when it is no longer (or not yet) in the ring */
int frame_timeline_ring_take(struct frame_timeline_ring *ring, uint64_t frame_id, struct frame_timeline_row *row);

enum
{
	FRAME_TIMELINE_METRIC_STATE_AGE,
	FRAME_TIMELINE_METRIC_TO_SUBMIT,
	FRAME_TIMELINE_METRIC_TO_SHOWN,
	FRAME_TIMELINE_METRIC_ENCODER_TAIL,
	FRAME_TIMELINE_METRIC_DISPLAY_WAIT,
	FRAME_TIMELINE_METRIC_LATE_START_WAIT,
	FRAME_TIMELINE_METRICS
};

/* debug.latency_log's window: FRAME_TIMELINE_WINDOW rows, then one line */
struct frame_timeline_latency
{
	unsigned frames, unshown;
	unsigned basis_seen; /* a bit per shown_basis */
	unsigned counts[FRAME_TIMELINE_METRICS];
	double values[FRAME_TIMELINE_METRICS][FRAME_TIMELINE_WINDOW]; /* the latency metrics, in milliseconds; FRAME_TIMELINE_METRIC_* */
};

/* adds a row; returns 1 when the window is full and frame_timeline_latency_line should print it */
int frame_timeline_latency_add(struct frame_timeline_latency *window, const struct frame_timeline_row *row);
/* the window's "latency: ..." line (no newline) into out, then an empty window; returns the line's length */
int frame_timeline_latency_line(struct frame_timeline_latency *window, const char *platform, char *out, size_t size);

#endif
