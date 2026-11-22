/*
FRAME_TIMELINE.C

The frame timeline's rows, late-join ring and latency line (frame_timeline.h).
*/

#include "frame_timeline.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void frame_timeline_row_clear(struct frame_timeline_row *row, uint64_t frame_id)
{
	row->frame_id = frame_id;
	row->shown_basis = FRAME_TIMELINE_SHOWN_NONE;
	row->controller_read = row->submit = row->shown = NAN;
	row->controller_state_age = row->display_wait = row->encoder_tail = row->late_start_wait = NAN;
}

void frame_timeline_ring_init(struct frame_timeline_ring *ring)
{
	unsigned index;

	pthread_mutex_init(&ring->lock, NULL);
	for (index = 0; index < FRAME_TIMELINE_RING; index++)
	{
		frame_timeline_row_clear(&ring->rows[index], 0);
		ring->early[index].frame_id = 0;
	}
}

void frame_timeline_ring_store(struct frame_timeline_ring *ring, const struct frame_timeline_row *row)
{
	unsigned slot = (unsigned)(row->frame_id % FRAME_TIMELINE_RING);

	pthread_mutex_lock(&ring->lock);
	ring->rows[slot] = *row;
	if (ring->early[slot].frame_id == row->frame_id)
	{
		ring->rows[slot].shown = ring->early[slot].shown;
		ring->rows[slot].shown_basis = ring->early[slot].basis;
	}
	ring->early[slot].frame_id = 0;
	pthread_mutex_unlock(&ring->lock);
}

void frame_timeline_ring_shown(struct frame_timeline_ring *ring, uint64_t frame_id, double shown, unsigned basis)
{
	struct frame_timeline_row *row = &ring->rows[frame_id % FRAME_TIMELINE_RING];

	if (!frame_id || shown <= 0.0)
		return;
	pthread_mutex_lock(&ring->lock);
	if (row->frame_id == frame_id)
	{
		row->shown = shown;
		row->shown_basis = basis;
	}
	else if (row->frame_id < frame_id)
	{
		ring->early[frame_id % FRAME_TIMELINE_RING].frame_id = frame_id;
		ring->early[frame_id % FRAME_TIMELINE_RING].shown = shown;
		ring->early[frame_id % FRAME_TIMELINE_RING].basis = basis;
	}
	pthread_mutex_unlock(&ring->lock);
}

int frame_timeline_ring_take(struct frame_timeline_ring *ring, uint64_t frame_id, struct frame_timeline_row *row)
{
	int found;

	if (!frame_id)
		return 0;
	pthread_mutex_lock(&ring->lock);
	found = ring->rows[frame_id % FRAME_TIMELINE_RING].frame_id == frame_id;
	if (found)
		*row = ring->rows[frame_id % FRAME_TIMELINE_RING];
	pthread_mutex_unlock(&ring->lock);
	return found;
}

static void add_metric(struct frame_timeline_latency *window, unsigned metric, double seconds)
{
	if (!isnan(seconds) && window->counts[metric] < FRAME_TIMELINE_WINDOW)
		window->values[metric][window->counts[metric]++] = seconds * 1000.0;
}

int frame_timeline_latency_add(struct frame_timeline_latency *window, const struct frame_timeline_row *row)
{
	add_metric(window, FRAME_TIMELINE_METRIC_STATE_AGE, row->controller_state_age);
	add_metric(window, FRAME_TIMELINE_METRIC_TO_SUBMIT, row->submit - row->controller_read);
	add_metric(window, FRAME_TIMELINE_METRIC_TO_SHOWN, row->shown - row->controller_read);
	add_metric(window, FRAME_TIMELINE_METRIC_ENCODER_TAIL, row->encoder_tail);
	add_metric(window, FRAME_TIMELINE_METRIC_DISPLAY_WAIT, row->display_wait);
	add_metric(window, FRAME_TIMELINE_METRIC_LATE_START_WAIT, row->late_start_wait);
	/* (frames without a shown time are counted, not dropped silently: encode spec revision 5 item 7) */
	if (isnan(row->shown))
		window->unshown++;
	window->basis_seen |= 1u << row->shown_basis;
	return ++window->frames >= FRAME_TIMELINE_WINDOW;
}

static int compare_doubles(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;

	return x < y ? -1 : x > y;
}

/* the nearest-rank percentile, or "-" for no values */
static int percentile(struct frame_timeline_latency *window, unsigned metric, double fraction, char *out, size_t size)
{
	unsigned count = window->counts[metric], rank;

	if (!count)
		return snprintf(out, size, "-");
	qsort(window->values[metric], count, sizeof(double), compare_doubles);
	rank = (unsigned)ceil(fraction * count);
	rank = rank < 1 ? 1 : rank > count ? count : rank;
	return snprintf(out, size, "%.2f", window->values[metric][rank - 1]);
}

int frame_timeline_latency_line(struct frame_timeline_latency *window, const char *platform, char *out, size_t size)
{
	static const struct { const char *name; unsigned metric; } metrics[] = {
		{ "controller_state_age", FRAME_TIMELINE_METRIC_STATE_AGE },
		{ "controller_to_submit", FRAME_TIMELINE_METRIC_TO_SUBMIT },
		{ "controller_to_shown", FRAME_TIMELINE_METRIC_TO_SHOWN },
		{ "encoder_tail", FRAME_TIMELINE_METRIC_ENCODER_TAIL },
		{ "display_wait", FRAME_TIMELINE_METRIC_DISPLAY_WAIT },
		{ "late_start_wait", FRAME_TIMELINE_METRIC_LATE_START_WAIT },
	};
	const char *basis = window->basis_seen & (1u << FRAME_TIMELINE_SHOWN_DRAWABLE_PRESENTED_TIME) ?
		"drawable_presented_time" : window->basis_seen & (1u << FRAME_TIMELINE_SHOWN_COMPOSITOR_PREDICTED) ?
		"compositor_predicted" : "none";
	int length = snprintf(out, size, "latency: frames=%u platform=%s shown_basis=%s", window->frames, platform, basis);
	unsigned index;

	for (index = 0; index < sizeof(metrics) / sizeof(metrics[0]); index++)
	{
		char p50[24], p95[24];

		percentile(window, metrics[index].metric, 0.50, p50, sizeof(p50));
		percentile(window, metrics[index].metric, 0.95, p95, sizeof(p95));
		if (length >= 0 && (size_t)length < size)
			length += snprintf(out + length, size - (size_t)length, " %s_p50=%s %s_p95=%s", metrics[index].name, p50,
				metrics[index].name, p95);
	}
	if (length >= 0 && (size_t)length < size)
		length += snprintf(out + length, size - (size_t)length, " unshown=%u", window->unshown);
	memset(window, 0, sizeof(*window));
	return length;
}
