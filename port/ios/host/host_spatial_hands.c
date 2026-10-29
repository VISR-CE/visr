/* The gesture recognizer (host_spatial_hands.h, declared): pinch hysteresis
with the tip-loss hold and its no-refire-on-return rule, the gun and off
hand's button and stick mapping, the drag and aim turns, and the prayer
sign's dwell and Start pulse. Plain C over the reduced joint facts only, so
port/ios/tests/hands_probe.c can run it under ASan and UBSan on a Mac;
host_spatial_hands.m supplies the joints and reads what this returns (Task
5's sampler wiring, not here). No host_logf: the caller logs the events. */
#include "host_spatial_hands.h"
#include "host_spatial_gamepad.h"
#include <math.h>
#include <string.h>

/* every timestamp this file is given (the `now` argument, and every value
derived from it: a pinch's lost_since, the prayer dwell's since, the pulse
and cooldown clocks) is assumed strictly positive, so 0.0 is free to mean
"idle" for every one of hands_state's own clocks below */

static void emit(struct hands_event *events, int capacity, int *count, int kind, int hand, int finger, int on,
	float value, float threshold)
{
	if (*count < capacity)
	{
		events[*count].kind = kind;
		events[*count].hand = hand;
		events[*count].finger = finger;
		events[*count].on = on;
		events[*count].value = value;
		events[*count].threshold = threshold;
	}
	(*count)++;
}

static float distance3(const float a[3], const float b[3])
{
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

	return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void sub3(const float a[3], const float b[3], float out[3])
{
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

static void cross3(const float a[3], const float b[3], float out[3])
{
	out[0] = a[1] * b[2] - a[2] * b[1];
	out[1] = a[2] * b[0] - a[0] * b[2];
	out[2] = a[0] * b[1] - a[1] * b[0];
}

static float dot3(const float a[3], const float b[3])
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void normalize3(float v[3])
{
	float length = sqrtf(dot3(v, v));

	if (length > 0.0f)
	{
		v[0] /= length;
		v[1] /= length;
		v[2] /= length;
	}
}

static float clampf(float value, float low, float high)
{
	if (value < low)
		return low;
	if (value > high)
		return high;
	return value;
}

/* one pinch's hysteresis and tip-loss hold (host_spatial_hands.h's struct
hands_pinch comment). hand_tracked governs the whole-hand immediate open
("A hand with tracked == 0 opens every pinch immediately"); thumb_tracked
and tip_tracked govern the per-tip hold-then-open. lost_since's sign tells
the three states apart: 0 idle, positive the untracked gap's start (timing
toward HOLD_SECONDS), negative pending (opened by a loss, waiting for a
tracked sample above OPEN before it may close again) */
static void pinch_update(struct hands_pinch *pinch, int hand, int finger, int hand_tracked, int thumb_tracked,
	const float thumb[3], int tip_tracked, const float tip[3], double now, struct hands_event *events, int capacity,
	int *count)
{
	float distance;

	if (!hand_tracked)
	{
		if (pinch->closed)
		{
			pinch->closed = 0;
			emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, 0.0f, HANDS_PINCH_OPEN_METERS);
		}
		pinch->lost_since = -1.0; /* pending: needs a confirmed open before it can close again */
		return;
	}

	if (!(thumb_tracked && tip_tracked))
	{
		if (pinch->lost_since == 0.0)
			pinch->lost_since = now; /* the gap starts here */
		else if (pinch->lost_since > 0.0 && pinch->closed && now - pinch->lost_since >= HANDS_PINCH_HOLD_SECONDS)
		{
			pinch->closed = 0;
			pinch->lost_since = -1.0;
			emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, 0.0f, HANDS_PINCH_OPEN_METERS);
		}
		return;
	}

	distance = distance3(thumb, tip);

	if (pinch->lost_since < 0.0)
	{
		/* pending: only a confirmed reading above OPEN clears it; closing is refused meanwhile */
		if (distance > HANDS_PINCH_OPEN_METERS)
			pinch->lost_since = 0.0;
		return;
	}
	if (pinch->lost_since > 0.0)
		pinch->lost_since = 0.0; /* the gap ended before the hold timed out: resume normally below */

	if (!pinch->closed && distance < HANDS_PINCH_CLOSE_METERS)
	{
		pinch->closed = 1;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 1, distance, HANDS_PINCH_CLOSE_METERS);
	}
	else if (pinch->closed && distance > HANDS_PINCH_OPEN_METERS)
	{
		pinch->closed = 0;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, distance, HANDS_PINCH_OPEN_METERS);
	}
}

/* the prayer sign's palm frame for one hand: centre, normal and finger
direction, built from the wrist and the index and little metacarpals only
(the joints ARKit never drops, per visr-hands-behaviour.md section 9).
The palm normal's sign is measured on device, not reasoned (Task 5's check,
visr-hands-spec.md rule 8); if the measurement disagrees with the formula
below, flip the sign here, nowhere else, since every caller of this
function reads the same convention */
static void palm_frame(const struct host_spatial_hand_joints *hand, float centre[3], float normal[3],
	float finger_direction[3])
{
	const float *wrist = hand->wrist, *index_mc = hand->metacarpal[0], *little_mc = hand->metacarpal[1];
	float index_dir[3], little_dir[3], mean_mc[3];
	int axis;

	for (axis = 0; axis < 3; axis++)
	{
		centre[axis] = (wrist[axis] + index_mc[axis] + little_mc[axis]) / 3.0f;
		mean_mc[axis] = (index_mc[axis] + little_mc[axis]) / 2.0f;
	}
	sub3(index_mc, wrist, index_dir);
	sub3(little_mc, wrist, little_dir);
	cross3(index_dir, little_dir, normal);
	normalize3(normal);
	sub3(mean_mc, wrist, finger_direction);
	normalize3(finger_direction);
}

/* both hands tracked with the wrist and both metacarpals: the prerequisite
the prayer sign needs before its geometry means anything */
static int prayer_ready(const struct host_spatial_hand_joints joints[2])
{
	int hand;

	for (hand = 0; hand < 2; hand++)
		if (!joints[hand].tracked || !joints[hand].metacarpal_tracked[0] || !joints[hand].metacarpal_tracked[1])
			return 0;
	return 1;
}

/* the prayer sign's pose test (visr-hands-spec.md rule 8, visr-hands-behaviour.md 6.1):
palm centres close, palm normals opposed, both hands' fingers pointed up */
static int prayer_holds(const struct host_spatial_hand_joints joints[2])
{
	float centre[2][3], normal[2][3], finger_direction[2][3];
	int hand;

	if (!prayer_ready(joints))
		return 0;
	for (hand = 0; hand < 2; hand++)
		palm_frame(&joints[hand], centre[hand], normal[hand], finger_direction[hand]);
	if (distance3(centre[0], centre[1]) > HANDS_PRAYER_DISTANCE_METERS)
		return 0;
	if (dot3(normal[0], normal[1]) > HANDS_PRAYER_OPPOSED_DOT)
		return 0;
	if (finger_direction[0][2] < HANDS_PRAYER_FINGERS_UP || finger_direction[1][2] < HANDS_PRAYER_FINGERS_UP)
		return 0;
	return 1;
}

int hands_recognize(struct hands_state *state, const struct host_spatial_hand_joints joints[2], int aiming_hand,
	const float aim_forward[3], int turn_snap, float deadzone_degrees, double now,
	struct host_spatial_controls controls[2], struct hands_event *events, int capacity)
{
	int count = 0;
	int g = aiming_hand ? 1 : 0, o = !g;
	int hand, finger;
	float drag_contribution = 0.0f, aim_contribution = 0.0f, stick_x;

	memset(controls, 0, 2 * sizeof(controls[0]));

	for (hand = 0; hand < 2; hand++)
	{
		const struct host_spatial_hand_joints *j = &joints[hand];

		for (finger = 0; finger < NUMBER_OF_HANDS_PINCHES; finger++)
		{
			int tip_index = finger + 1; /* tip[0] is the thumb */

			pinch_update(&state->pinch[hand][finger], hand, finger, j->tracked, j->tip_tracked[0], j->tip[0],
				j->tip_tracked[tip_index], j->tip[tip_index], now, events, capacity, &count);
		}
	}

	/* the gun hand's buttons, and the off hand's stick and click */
	controls[g].trigger = state->pinch[g][HANDS_PINCH_INDEX].closed ? 1.0f : 0.0f;
	controls[g].face_low = state->pinch[g][HANDS_PINCH_RING].closed ? 1 : 0;
	if (state->pinch[o][HANDS_PINCH_INDEX].closed)
		controls[o].stick_y = 1.0f;
	else if (state->pinch[o][HANDS_PINCH_MIDDLE].closed)
		controls[o].stick_y = -1.0f;
	controls[o].stick_click = state->pinch[o][HANDS_PINCH_LITTLE].closed ? 1 : 0;

	if (state->pinch[g][HANDS_PINCH_MIDDLE].closed)
	{
		float lateral, sign, magnitude;

		/* drag_anchored is false exactly from the pinch's open until its next close (set below,
		in the else branch), so this is the close transition: a fresh anchor and a fresh latch */
		if (!state->drag_anchored)
		{
			state->drag_anchor[0] = joints[g].wrist[0];
			state->drag_anchor[1] = joints[g].wrist[1];
			state->drag_anchor[2] = joints[g].wrist[2];
			state->drag_anchored = 1;
			state->drag_snapped = 0;
		}
		/* lateral: the drag along the game's left axis, negated so right reads positive */
		lateral = -(joints[g].wrist[1] - state->drag_anchor[1]);
		sign = lateral < 0.0f ? -1.0f : 1.0f;
		magnitude = fabsf(lateral);

		if (turn_snap)
		{
			if (magnitude > HANDS_DRAG_START_METERS && !state->drag_snapped)
			{
				state->pulse_until = now + HANDS_PULSE_SECONDS;
				state->pulse_x = sign;
				state->drag_snapped = 1;
				emit(events, capacity, &count, HANDS_EVENT_DRAG, g, HANDS_PINCH_MIDDLE, 1, magnitude,
					HANDS_DRAG_START_METERS);
			}
		}
		else
			drag_contribution = sign * (magnitude - HANDS_DRAG_START_METERS) /
				(HANDS_DRAG_FULL_METERS - HANDS_DRAG_START_METERS);
	}
	else
	{
		float yaw, excess, turn_sign;

		state->drag_anchored = 0; /* the pinch is open: the next close picks a fresh anchor */

		yaw = atan2f(aim_forward[1], aim_forward[0]) * (180.0f / (float)M_PI);
		excess = fabsf(yaw) - deadzone_degrees;
		turn_sign = yaw < 0.0f ? -1.0f : 1.0f;

		if (turn_snap)
		{
			if (excess < 0.0f)
				state->aim_turn_armed = 1;
			else if (excess > 0.0f && state->aim_turn_armed)
			{
				state->pulse_until = now + HANDS_PULSE_SECONDS;
				state->pulse_x = -turn_sign;
				state->aim_turn_armed = 0;
				emit(events, capacity, &count, HANDS_EVENT_AIM_TURN, g, 0, 1, yaw, deadzone_degrees);
			}
		}
		else
			aim_contribution = -turn_sign *
				clampf(excess / (HANDS_AIM_TURN_FULL_DEGREES - deadzone_degrees), 0.0f, 1.0f);
	}

	/* snap mode is pulse-only (no smooth fallback); smooth mode sums the two sources; either
	way a live pulse overrides, since drag and aim turn never both run in the same sample */
	stick_x = turn_snap ? 0.0f : clampf(drag_contribution + aim_contribution, -1.0f, 1.0f);
	if (now < state->pulse_until)
		stick_x = state->pulse_x;
	controls[g].stick_x = stick_x;

	/* the prayer sign: both hands held together fires the Start pulse once per dwell */
	if (prayer_holds(joints))
	{
		if (state->prayer_since == 0.0)
		{
			state->prayer_since = now;
			state->prayer_fired = now < state->prayer_cooldown_until; /* a hold begun inside the
				cooldown is consumed without firing, even once the cooldown itself ends */
		}
		if (!state->prayer_fired && now - state->prayer_since >= HANDS_PRAYER_DWELL_SECONDS)
		{
			state->start_until = now + HANDS_PULSE_SECONDS;
			state->prayer_fired = 1;
			state->prayer_cooldown_until = now + HANDS_PRAYER_COOLDOWN_SECONDS;
			emit(events, capacity, &count, HANDS_EVENT_PRAYER, 0, 0, 1, (float)(now - state->prayer_since),
				(float)HANDS_PRAYER_DWELL_SECONDS);
			emit(events, capacity, &count, HANDS_EVENT_START, g, 0, 1, 1.0f, (float)HANDS_PULSE_SECONDS);
		}
	}
	else
	{
		state->prayer_since = 0.0;
		state->prayer_fired = 0;
	}
	controls[g].menu = now < state->start_until;

	/* tracked == 0 zeros every control of that hand, the same sample, regardless of anything
	computed above (visr's Task 4 controller ruling 1) */
	if (!joints[0].tracked)
		memset(&controls[0], 0, sizeof(controls[0]));
	if (!joints[1].tracked)
		memset(&controls[1], 0, sizeof(controls[1]));

	return count;
}
