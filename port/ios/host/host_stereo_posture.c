/* The player's posture from the head's height (host_stereo_posture.h). */
#include "host_stereo_posture.h"

#include <stdlib.h>
#include <string.h>

static enum host_stereo_posture_kind classify(float height, enum host_stereo_posture_kind current)
{
	if (height < POSTURE_FLOOR_PLAUSIBLE_LOW || height > POSTURE_FLOOR_PLAUSIBLE_HIGH)
		return HOST_POSTURE_NO_FLOOR;
	if (current == HOST_POSTURE_STANDING)
		return height < POSTURE_STANDING_HEIGHT - POSTURE_HYSTERESIS ? HOST_POSTURE_SEATED : HOST_POSTURE_STANDING;
	if (current == HOST_POSTURE_SEATED)
		return height > POSTURE_STANDING_HEIGHT + POSTURE_HYSTERESIS ? HOST_POSTURE_STANDING : HOST_POSTURE_SEATED;
	return height > POSTURE_STANDING_HEIGHT ? HOST_POSTURE_STANDING : HOST_POSTURE_SEATED;
}

static int compare_floats(const void *a, const void *b)
{
	float x = *(const float *)a, y = *(const float *)b;

	return (x > y) - (x < y);
}

void host_stereo_posture_start(struct host_stereo_posture *posture, double now)
{
	memset(posture, 0, sizeof(*posture));
	posture->estimating = 1;
	posture->window_start = now;
	posture->posture = HOST_POSTURE_UNKNOWN;
	posture->crossed_since = -1.0;
}

int host_stereo_posture_sample(struct host_stereo_posture *posture, double now, float height, float pitch_degrees,
	struct host_stereo_posture_event *event)
{
	memset(event, 0, sizeof(*event));
	if (posture->estimating)
	{
		if (pitch_degrees < -POSTURE_PITCH_LIMIT_DEGREES)
			posture->skipped++;
		else if (posture->sample_count < POSTURE_MAXIMUM_SAMPLES)
			posture->samples[posture->sample_count++] = height;
		if (now - posture->window_start < POSTURE_WINDOW_SECONDS || posture->sample_count == 0)
			return 0;
		qsort(posture->samples, (size_t)posture->sample_count, sizeof(float), compare_floats);
		posture->height = posture->sample_count % 2 ? posture->samples[posture->sample_count / 2] :
			0.5f * (posture->samples[posture->sample_count / 2 - 1] + posture->samples[posture->sample_count / 2]);
		posture->posture = classify(posture->height, HOST_POSTURE_UNKNOWN);
		posture->estimating = 0;
		event->decided = 1;
		event->posture = posture->posture;
		event->height = posture->height;
		event->sample_count = posture->sample_count;
		event->skipped = posture->skipped;
		return 1;
	}
	if (posture->posture == HOST_POSTURE_UNKNOWN || pitch_degrees < -POSTURE_PITCH_LIMIT_DEGREES)
		return 0;
	if (classify(height, posture->posture) == posture->posture)
	{
		posture->crossed_since = -1.0;
		return 0;
	}
	if (posture->crossed_since < 0.0)
		posture->crossed_since = now;
	if (now - posture->crossed_since < POSTURE_CHANGE_SECONDS)
		return 0;
	posture->posture = classify(height, posture->posture);
	posture->height = height;
	posture->crossed_since = -1.0;
	event->decided = 2;
	event->posture = posture->posture;
	event->height = height;
	return 2;
}

const char *host_stereo_posture_name(enum host_stereo_posture_kind posture)
{
	switch (posture)
	{
	case HOST_POSTURE_SEATED: return "seated";
	case HOST_POSTURE_STANDING: return "standing";
	case HOST_POSTURE_NO_FLOOR: return "seated (the height is no floor reference)";
	default: return "unknown";
	}
}
