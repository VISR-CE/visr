/* The player's posture in head-tracked stereo, estimated from the head's
height above ARKit's origin (host_stereo.m). Log only: nothing reads the
estimate yet. It tells whether that origin is a usable floor reference
before any standing behavior depends on it (comfort and aim design,
phase 0). Plain C, so a host-side test can check it
(port/ios/tests/stereo_posture_probe.c). */
#ifndef HOST_STEREO_POSTURE_H
#define HOST_STEREO_POSTURE_H

/* a head above this height, in meters over the floor, is standing */
#define POSTURE_STANDING_HEIGHT 1.35f
/* the posture changes only once the height is this far past the threshold */
#define POSTURE_HYSTERESIS 0.10f
/* heights outside this range, in meters, mean the origin is no floor */
#define POSTURE_FLOOR_PLAUSIBLE_LOW 0.6f
#define POSTURE_FLOOR_PLAUSIBLE_HIGH 2.2f
/* seconds of samples the first estimate takes its median over */
#define POSTURE_WINDOW_SECONDS 2.0f
/* seconds the height must stay past the threshold for a change */
#define POSTURE_CHANGE_SECONDS 3.0f
/* a sample with the head pitched further down than this (degrees) is left out */
#define POSTURE_PITCH_LIMIT_DEGREES 45.0f
/* the most samples one window holds (90 Hz for the window, with room) */
#define POSTURE_MAXIMUM_SAMPLES 256

enum host_stereo_posture_kind
{
	HOST_POSTURE_UNKNOWN,
	HOST_POSTURE_SEATED,
	HOST_POSTURE_STANDING,
	/* the height is outside the plausible range: the seated fallback */
	HOST_POSTURE_NO_FLOOR,
};

struct host_stereo_posture
{
	int estimating;                        /* the first window is collecting */
	double window_start;                   /* when it began, seconds */
	float samples[POSTURE_MAXIMUM_SAMPLES];
	int sample_count, skipped;             /* kept, and left out for pitch */
	enum host_stereo_posture_kind posture; /* the last decision */
	float height;                          /* the height it was taken at, meters */
	double crossed_since;                  /* when the height went past the threshold; < 0 when not */
};

/* the outcome of one sample, for the log */
struct host_stereo_posture_event
{
	int decided;                           /* 1: the first estimate; 2: a change */
	enum host_stereo_posture_kind posture;
	float height;                          /* the median (first) or the current height (change) */
	int sample_count, skipped;
};

/* starts a new first estimate (Enter VR, a recenter, a new level's first frame) */
void host_stereo_posture_start(struct host_stereo_posture *posture, double now);
/* one head sample: its height over the origin in meters and its pitch in
degrees (negative looks down). Returns the event's decided field */
int host_stereo_posture_sample(struct host_stereo_posture *posture, double now, float height, float pitch_degrees,
	struct host_stereo_posture_event *event);
const char *host_stereo_posture_name(enum host_stereo_posture_kind posture);

#endif
