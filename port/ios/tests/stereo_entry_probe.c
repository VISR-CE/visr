/* host_stereo_entry.h and halo_stereo_mode_entered: entering and leaving VR
on Apple Vision Pro while the game runs. Enter VR over display.stereo "off"
(or a mode the game doesn't recognize, which it reads as off) is HEAD mode;
"head", "screen" and "side_by_side" stay as the player set them, entered or
not; leaving VR returns display.stereo's mode. Back and Start held together
switch once per hold, after HOST_STEREO_CHORD_SECONDS. */
#include <assert.h>
#include <stdio.h>

#include "host_stereo_entry.h"

int main(void)
{
	struct host_stereo_chord chord = {0};
	int switches = 0;
	double now;

	/* display.stereo's names, as the game reads them */
	assert(host_stereo_mode_named("off") == HALO_STEREO_OFF);
	assert(host_stereo_mode_named("head") == HALO_STEREO_HEAD);
	assert(host_stereo_mode_named("screen") == HALO_STEREO_SCREEN);
	assert(host_stereo_mode_named("side_by_side") == HALO_STEREO_SIDE_BY_SIDE);
	assert(host_stereo_mode_named("Head") == HALO_STEREO_OFF);
	assert(host_stereo_mode_named("") == HALO_STEREO_OFF);
	assert(host_stereo_mode_named(NULL) == HALO_STEREO_OFF);

	/* a player who never presses Enter VR keeps display.stereo's mode */
	assert(halo_stereo_mode_entered(HALO_STEREO_OFF, 0) == HALO_STEREO_OFF);
	assert(halo_stereo_mode_entered(HALO_STEREO_HEAD, 0) == HALO_STEREO_HEAD);
	assert(halo_stereo_mode_entered(HALO_STEREO_SCREEN, 0) == HALO_STEREO_SCREEN);
	/* Enter VR over "off" is head-tracked stereo */
	assert(halo_stereo_mode_entered(host_stereo_mode_named("off"), 1) == HALO_STEREO_HEAD);
	assert(halo_stereo_mode_entered(host_stereo_mode_named("bogus"), 1) == HALO_STEREO_HEAD);
	/* and an explicit mode stays: the 3D screen stays the 3D screen */
	assert(halo_stereo_mode_entered(HALO_STEREO_HEAD, 1) == HALO_STEREO_HEAD);
	assert(halo_stereo_mode_entered(HALO_STEREO_SCREEN, 1) == HALO_STEREO_SCREEN);
	assert(halo_stereo_mode_entered(HALO_STEREO_SIDE_BY_SIDE, 1) == HALO_STEREO_SIDE_BY_SIDE);

	/* the chord: nothing until held long enough */
	assert(!host_stereo_chord_step(&chord, 0, 10.0));
	assert(!host_stereo_chord_step(&chord, 1, 10.0));
	assert(!host_stereo_chord_step(&chord, 1, 10.0 + HOST_STEREO_CHORD_SECONDS - 0.01));
	/* then once, however long the hold lasts */
	for (now = 11.0; now < 20.0; now += 1.0 / 60.0)
		switches += host_stereo_chord_step(&chord, 1, now);
	assert(switches == 1);
	/* a short tap after letting go does nothing; a second full hold switches back */
	assert(!host_stereo_chord_step(&chord, 0, 20.0));
	assert(!host_stereo_chord_step(&chord, 1, 20.1));
	assert(!host_stereo_chord_step(&chord, 0, 20.5));
	assert(!host_stereo_chord_step(&chord, 1, 21.0));
	assert(host_stereo_chord_step(&chord, 1, 21.0 + HOST_STEREO_CHORD_SECONDS));
	/* letting go of either button for a poll restarts the hold */
	assert(!host_stereo_chord_step(&chord, 0, 23.0));
	assert(!host_stereo_chord_step(&chord, 1, 23.0));
	assert(!host_stereo_chord_step(&chord, 1, 23.6));
	assert(!host_stereo_chord_step(&chord, 0, 23.7));
	assert(!host_stereo_chord_step(&chord, 1, 23.8));
	assert(!host_stereo_chord_step(&chord, 1, 24.5));
	assert(host_stereo_chord_step(&chord, 1, 24.8));

	printf("stereo entry: Enter VR's mode and the controller's chord OK\n");
	return 0;
}
