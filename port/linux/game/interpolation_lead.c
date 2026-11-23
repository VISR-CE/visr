/* SPDX-License-Identifier: Apache-2.0 */
/* The display lead's correction (interpolation_lead.h) */
#include "interpolation_lead.h"

/* the running average's weight per frame: about 20 frames to adapt */
#define INTERPOLATION_LEAD_WEIGHT 0.05

void interpolation_lead_reset(struct interpolation_lead *state)
{
	state->average = -1.0;
	state->correction[0] = state->correction[1] = 0.0;
	state->corrected[0] = state->corrected[1] = 0;
	state->averaged_frame = 0;
	state->lead = 0.0;
}

double interpolation_lead_correction(struct interpolation_lead *state, unsigned long frame, int ticked,
	double clock_read, double presentation)
{
	int kind = ticked ? 1 : 0;
	double lead;

	if (presentation == 0.0)
		return state->corrected[kind] ? state->correction[kind] : 0.0;
	lead = presentation - clock_read;
	if (lead < 0.0)
		lead = 0.0;
	if (state->average < 0.0)
	{
		state->average = lead;
		state->averaged_frame = frame;
	}
	else if (frame != state->averaged_frame)
	{
		state->average += (lead - state->average) * INTERPOLATION_LEAD_WEIGHT;
		state->averaged_frame = frame;
	}
	state->lead = lead;
	state->correction[kind] = lead - state->average;
	state->corrected[kind] = 1;
	return state->correction[kind];
}
