/* Entering and leaving VR on Apple Vision Pro while the game runs: the
window's Enter VR ornament, the Exit VR panel in the immersive space and the
controller's chord (Back and Start held) all switch between the window and
the space (host_theater.m). The rules here are pure C so
port/ios/tests/stereo_entry_probe.c runs them. */
#ifndef HOST_STEREO_ENTRY_H
#define HOST_STEREO_ENTRY_H

#include <string.h>

#include "halo_stereo.h"

/* how long Back and Start are held together to switch */
#define HOST_STEREO_CHORD_SECONDS 1.0

/* the mode a display.stereo string names, as the game reads it
(port/linux/game/stereo.c): anything unrecognized is off */
static inline int host_stereo_mode_named(const char *name)
{
	if (name && !strcmp(name, "head"))
		return HALO_STEREO_HEAD;
	if (name && !strcmp(name, "screen"))
		return HALO_STEREO_SCREEN;
	if (name && !strcmp(name, "side_by_side"))
		return HALO_STEREO_SIDE_BY_SIDE;
	return HALO_STEREO_OFF;
}

/* the controller's chord: one switch for each hold of at least
HOST_STEREO_CHORD_SECONDS, however long the hold lasts */
struct host_stereo_chord
{
	double began;   /* when the hold began; 0 while not held */
	int fired;      /* this hold has switched already */
};

/* each event poll: held says whether a controller has Back and Start down
together, now is the time in seconds (above 0); returns 1 once per hold, when
the hold reaches HOST_STEREO_CHORD_SECONDS */
static inline int host_stereo_chord_step(struct host_stereo_chord *chord, int held, double now)
{
	if (!held)
	{
		chord->began = 0.0;
		chord->fired = 0;
		return 0;
	}
	if (chord->began == 0.0)
		chord->began = now;
	if (chord->fired || now - chord->began < HOST_STEREO_CHORD_SECONDS)
		return 0;
	chord->fired = 1;
	return 1;
}

#endif
