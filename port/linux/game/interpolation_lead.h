/* SPDX-License-Identifier: Apache-2.0 */
#ifndef INTERPOLATION_LEAD_H
#define INTERPOLATION_LEAD_H

/* How far ahead of its presentation a frame read the game clock, and the blend
correction that follows from it (render_interpolation.c, fraction_for_display).

The game clock is read once a loop (main_update_time_unthrottled, source/main/main.c)
and every frame's tick fraction comes from that read. The frame is seen at its
presentation, some time later. The lead is that gap: presentation time minus the
moment the loop clock was read. Measured from anywhere later (after the tick ran,
after the Compositor's wait) it reads short by whatever ran in between, and a tick
frame reads shorter than a frame without one.

The blend is advanced by the lead's distance from its running average, so the
world moves by the same amount between any two presentations. The average
adapts over about 20 frames and is taken once per loop frame, however many
times the fraction is asked for.

A frame may ask before its presentation is known (in stereo, game_time_update's
call around game_frame comes before the Compositor's frame opens). It then gets
the last correction of a frame of the same kind (one that ran a tick, or one that
did not), so the effects game_frame hangs off objects land where the frame draws
them; the average is left alone. */

struct interpolation_lead
{
	double average;                    /* milliseconds; negative: none yet */
	double correction[2];              /* the last correction, by kind (1: the frame ran a tick) */
	int corrected[2];                  /* whether correction[kind] holds one */
	unsigned long averaged_frame;      /* the loop frame whose lead the average took */
	double lead;                       /* the last measured lead (debug.frame_trace) */
};

void interpolation_lead_reset(struct interpolation_lead *state);

/* The correction in milliseconds for loop frame FRAME (lead minus average;
positive: this frame shows later after its clock read than usual).
CLOCK_READ and PRESENTATION are on one clock, in milliseconds; PRESENTATION 0:
not known yet (the estimate above). A presentation before the clock read (a
load, a hitch) counts as a zero lead. */
double interpolation_lead_correction(struct interpolation_lead *state, unsigned long frame, int ticked,
	double clock_read, double presentation);

#endif
