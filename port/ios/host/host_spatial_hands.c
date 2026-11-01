/* The gesture recognizer (host_spatial_hands.h, declared): pinch hysteresis
over fingertips carried in the palm's frame through ARKit's dropouts, the
check on the joints behind a pinch that releases it when the fingers part
rather than when the hidden fingertip is found again, the gun and off hand's
button and stick mapping, the aim turn, and the prayer sign's dwell and Start
pulse. Plain C over the reduced joint facts only, so
port/ios/tests/hands_probe.c can run it under ASan and UBSan on a Mac;
host_spatial_hands.m supplies the joints and reads what this returns. No
host_logf: the caller logs the events. */
#include "host_spatial_hands.h"
#include "host_spatial_gamepad.h"
#include <math.h>
#include <string.h>

/* every timestamp this file is given (the `now` argument, and every value
derived from it: a pinch's carried_since, the prayer dwell's since, the pulse
and cooldown clocks) is assumed strictly positive, so 0.0 is free to mean
"idle" for every one of hands_state's own clocks below */

/* path is HANDS_PATH_* (host_spatial_hands.h): which reading moved a pinch, and
HANDS_PATH_NONE for every event of a kind that has no such reading */
static void emit(struct hands_event *events, int capacity, int *count, int kind, int hand, int finger, int on,
	float value, float threshold, int path)
{
	if (*count < capacity)
	{
		events[*count].kind = kind;
		events[*count].hand = hand;
		events[*count].finger = finger;
		events[*count].on = on;
		events[*count].value = value;
		events[*count].threshold = threshold;
		events[*count].path = path;
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

/* a value held between low and high, with the NaN guard host_spatial_gamepad.c's stick() has:
a NaN compares false against both ends, so without it a 0 / 0 from the aim turn's ramp would
pass straight through into controls[g].stick_x. Returning low answers both callers: the
ramp's own floor is "no turn", and the stick's clamp runs from -1 but is handed one already
clamped ramp, so no NaN can reach it */
static float clampf(float value, float low, float high)
{
	if (!(value == value))
		return low;
	if (value < low)
		return low;
	if (value > high)
		return high;
	return value;
}

/* the prayer sign's palm frame for one hand: center, normal and finger
direction, built from the wrist and the index and little metacarpals only,
the joints that stay tracked when two hands come together. The palm normal's
sign is measured on device, not reasoned; if the measurement disagrees with
the formula below, flip the sign here, nowhere else, since every caller of
this function reads the same convention */
static void palm_frame(const struct host_spatial_hand_joints *hand, float center[3], float normal[3],
	float finger_direction[3])
{
	const float *wrist = hand->wrist, *index_mc = hand->metacarpal[0], *little_mc = hand->metacarpal[1];
	float index_dir[3], little_dir[3], mean_mc[3];
	int axis;

	for (axis = 0; axis < 3; axis++)
	{
		center[axis] = (wrist[axis] + index_mc[axis] + little_mc[axis]) / 3.0f;
		mean_mc[axis] = (index_mc[axis] + little_mc[axis]) / 2.0f;
	}
	sub3(index_mc, wrist, index_dir);
	sub3(little_mc, wrist, little_dir);
	cross3(index_dir, little_dir, normal);
	normalize3(normal);
	sub3(mean_mc, wrist, finger_direction);
	normalize3(finger_direction);
}

/* the same three joints as an orthonormal frame a position can be stored in:
the wrist for the origin, palm_frame's own normal and finger direction for
two of the axes (the normal is the cross product of two vectors the finger
direction is a combination of, so the two are already square), and their
cross product for the third */
struct palm_basis { float origin[3], axis[3][3]; };

/* 1 and the frame, or 0 where the three joints give no plane to build it on
(the metacarpals collinear with the wrist, which a real hand never is) */
static int palm_basis_build(const struct host_spatial_hand_joints *hand, struct palm_basis *basis)
{
	float center[3];

	memcpy(basis->origin, hand->wrist, sizeof(basis->origin));
	palm_frame(hand, center, basis->axis[0], basis->axis[1]);
	cross3(basis->axis[0], basis->axis[1], basis->axis[2]);
	/* two unit axes square to each other cross to a unit third; anything well
	short of that means normalize3 found nothing to normalize */
	return dot3(basis->axis[2], basis->axis[2]) > 0.5f;
}

/* a point into one palm frame's coordinates, and back out of them: what a
carried fingertip is stored and read as */
static void palm_encode(const struct palm_basis *basis, const float point[3], float local[3])
{
	float offset[3];
	int axis;

	sub3(point, basis->origin, offset);
	for (axis = 0; axis < 3; axis++)
		local[axis] = dot3(offset, basis->axis[axis]);
}

static void palm_decode(const struct palm_basis *basis, const float local[3], float point[3])
{
	int axis, component;

	for (component = 0; component < 3; component++)
		point[component] = basis->origin[component];
	for (axis = 0; axis < 3; axis++)
		for (component = 0; component < 3; component++)
			point[component] += local[axis] * basis->axis[axis][component];
}

/* a hand ARKit has lost for longer than its gap carries nothing forward: the
hand that comes back builds its carry from what it is seen doing then, rather
than from where its fingers were before it left (hand_lost below, and the
same sample's pinch_update, which opens every one of that hand's pinches).
A hand still inside its gap keeps all five, since it has not been lost yet */
static void tips_forget(struct hands_state *state, int hand)
{
	memset(state->tip_local[hand], 0, sizeof(state->tip_local[hand]));
	memset(state->tip_known[hand], 0, sizeof(state->tip_known[hand]));
}

/* one tracked hand's five fingertips as this sample reads them: a tip ARKit
reports as given, with its palm-frame coordinates stored for later; a tip
ARKit has dropped from those stored coordinates, read back through the palm's
current pose, so the pinch on it keeps measuring and a hand that moves while
the tip is out of sight carries the tip with it. usable is 0 only where
neither is possible, which is a tip not yet seen or a palm frame the
metacarpals are too lost to build, and a pinch on an unusable tip holds
whatever state it had (pinch_update) */
static void tips_resolve(struct hands_state *state, int hand, const struct host_spatial_hand_joints *joints,
	float tip[5][3], int usable[5], int carried[5])
{
	struct palm_basis basis;
	int framed = joints->metacarpal_tracked[0] && joints->metacarpal_tracked[1] &&
		palm_basis_build(joints, &basis);
	int index;

	for (index = 0; index < 5; index++)
	{
		if (joints->tip_tracked[index])
		{
			memcpy(tip[index], joints->tip[index], sizeof(tip[index]));
			usable[index] = 1;
			if (framed)
			{
				palm_encode(&basis, tip[index], state->tip_local[hand][index]);
				state->tip_known[hand][index] = 1;
			}
			continue;
		}
		usable[index] = framed && state->tip_known[hand][index];
		if (usable[index])
		{
			palm_decode(&basis, state->tip_local[hand][index], tip[index]);
			carried[index] = 1;
		}
	}
}

/* whether one hand counts as gone this sample, which is what everything
below acts on rather than the anchor's own tracked flag. ARKit can stop
reporting a whole hand for a sample or two, and reading that as a hand put
down turns one held pinch into a release and an immediate fresh press: a
trigger pulled twice, a walk-forward stutter. So a hand that stops being
reported starts a gap instead and holds everything it had through it, exactly
as a fingertip out of sight is carried (tips_resolve), and only once
HANDS_HAND_GAP_SECONDS has run does it become a loss. A hand back inside its
gap resumes with no event at all, since nothing about it ever moved.

What the gap must not hold is anything read out of the joints themselves:
hands_recognize keeps the aim turn and the prayer sign on the live flag,
whose geometry is stale the moment ARKit stops reporting it.

Called once per hand per sample: it keeps the gap's start time */
static int hand_lost(struct hands_state *state, int hand, int tracked, double now)
{
	if (tracked)
	{
		state->hand_gap_since[hand] = 0.0;
		return 0;
	}
	if (state->hand_gap_since[hand] == 0.0)
		state->hand_gap_since[hand] = now;
	return now - state->hand_gap_since[hand] >= HANDS_HAND_GAP_SECONDS;
}

/* one pinch's hysteresis (host_spatial_hands.h's struct hands_pinch
comment). hand_tracked governs the whole-hand open: a hand ARKit is not
reporting opens every pinch, which hands_recognize asks for only once that
hand's gap has run out (hand_lost), so a blink reaches this function not at
all. Below that there is nothing but the hysteresis, over whichever of the
two positions tips_resolve could put together, with part_decide ahead of it
wherever one of those two is a guess. A fingertip going out of sight is not a
reason to open either: ARKit loses the outer tips for anything from a
twentieth of a second to tens of seconds while the hand is plainly held
still, and every one of those dropouts used to open the pinch the player was
holding */
/* one pinch's still-visible pair as this sample reads it, per level
(host_spatial_hands.h's HANDS_PART_LEVEL_*). No carry and no palm frame: the whole reason
these joints are the ones to ask is that ARKit keeps reporting them while the thumb hides
the fingertip, so a level either has both its ends this sample or is not used. finger is the
pinch's own index, and its chain of joints is finger + 1, as its tip is */
struct part_pair { int known[NUMBER_OF_HANDS_PART_LEVELS]; float distance[NUMBER_OF_HANDS_PART_LEVELS]; };

static void part_pair_of(const struct host_spatial_hand_joints *joints, int finger, struct part_pair *pair)
{
	int chain = finger + 1;
	const float *thumb[NUMBER_OF_HANDS_PART_LEVELS];
	int thumb_tracked[NUMBER_OF_HANDS_PART_LEVELS];
	int level;

	thumb[HANDS_PART_LEVEL_THUMB_INTERMEDIATE_TIP] = joints->intermediate_tip[0];
	thumb[HANDS_PART_LEVEL_THUMB_INTERMEDIATE_BASE] = joints->intermediate_base[0];
	thumb_tracked[HANDS_PART_LEVEL_THUMB_INTERMEDIATE_TIP] = joints->intermediate_tip_tracked[0];
	thumb_tracked[HANDS_PART_LEVEL_THUMB_INTERMEDIATE_BASE] = joints->intermediate_base_tracked[0];
	for (level = 0; level < NUMBER_OF_HANDS_PART_LEVELS; level++)
	{
		pair->known[level] = joints->intermediate_base_tracked[chain] && thumb_tracked[level];
		pair->distance[level] = pair->known[level] ?
			distance3(joints->intermediate_base[chain], thumb[level]) : 0.0f;
	}
}

/* what the part check measures growth from: every level this sample can see, recorded on
every sample both of the pinch's fingertips are ARKit's own reading, so the reference always
holds the pair's separation in the pose those tips confirmed the fingers were in. A level
this sample cannot see is forgotten rather than left standing, since a reference taken in an
older pose describes a hand shape that has since moved on. The pair's state goes with it: the
tips decide for themselves while they can be seen, so whatever the pair said during a gap is
spent as soon as that gap ends */
static void part_reference_record(struct hands_pinch *pinch, const struct part_pair *pair)
{
	int level;

	for (level = 0; level < NUMBER_OF_HANDS_PART_LEVELS; level++)
	{
		pinch->part_reference_known[level] = pair->known[level];
		pinch->part_reference[level] = pair->known[level] ? pair->distance[level] : 0.0f;
	}
	pinch->parted = 0;
}

/* the fix the carry needed. A pinch occludes the very fingertip its distance is measured
from, because the thumb is sitting on it, so the tip goes missing on the sample after the
press and the carry answers "still where it was, which is on the thumb" for the whole
dropout. For a finger genuinely away from the thumb and merely out of view that is the right
answer and the carry stays. For a finger the thumb is on it cannot be: the state being
measured is what causes the occlusion, so the carry can never notice the fingers parting, and
on device a tap meant as one shot became a held trigger for as long as 2.3 s.

So while a fingertip cannot be measured, the pinch is judged instead by the pair of joints
behind the contact point that the thumb is not sitting on, against what that pair measured
while the tips were last seen (part_reference_record). Returns 1 when the carry may not be
trusted this sample, which releases a closed pinch and keeps an already-released one open: a
pinch is never closed again by a position carried from a pose the pair says the hand has
left. There is no clock in any of it, which is what lets a pinch be held for as long as the
player holds it (hands_probe.c's pinch_hold_to_fire_never_releases) */
static int part_decide(struct hands_pinch *pinch, const struct part_pair *pair, int hand, int finger,
	struct hands_event *events, int capacity, int *count)
{
	int level;

	for (level = 0; level < NUMBER_OF_HANDS_PART_LEVELS; level++)
	{
		float growth;

		if (!(pair->known[level] && pinch->part_reference_known[level]))
			continue;
		growth = pair->distance[level] - pinch->part_reference[level];
		/* a band, as this file's every other threshold is: past the margin the fingers are
		apart, and only back under half of it is a carried position trusted to close the pinch
		again. Without the lower edge a pair sitting at the margin would release and re-close
		sample after sample, turning one held pinch into a stream of shots */
		if (growth > HANDS_PART_MARGIN_METERS)
			pinch->parted = 1;
		else if (growth < HANDS_PART_MARGIN_METERS / 2.0f)
			pinch->parted = 0;
		if (pinch->parted && pinch->closed)
		{
			pinch->closed = 0;
			emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, growth,
				HANDS_PART_MARGIN_METERS, HANDS_PATH_PARTED);
		}
		return pinch->parted;
	}
	/* no level this sample can both see and compare against: the carry's answer stands, which
	is a closed pinch holding, exactly as before this check existed. A release the pair has
	already decided stands too, since the pair going missing is not the fingers coming back
	together */
	return pinch->parted;
}

/* the end of a carried gap, for the log: how long this pinch went without a
live reading of both its ends. Silent unless a gap was running, so every
caller below can hand it the state it is about to move on. Its path says
whether the pair ended up releasing the pinch inside that gap, which is the
one thing about a gap worth knowing beyond its length */
static void miss_ended(const struct hands_pinch *pinch, int hand, int finger, double now,
	struct hands_event *events, int capacity, int *count)
{
	if (pinch->carried_since <= 0.0)
		return;
	emit(events, capacity, count, HANDS_EVENT_PINCH_MISS, hand, finger, 0, (float)(now - pinch->carried_since), 0.0f,
		pinch->parted ? HANDS_PATH_PARTED : HANDS_PATH_CARRIED);
}

static void pinch_update(struct hands_pinch *pinch, int hand, int finger, int hand_tracked, int thumb_usable,
	const float thumb[3], int tip_usable, const float tip[3], int carried, const struct part_pair *pair,
	double now, struct hands_event *events, int capacity, int *count)
{
	float distance;
	/* this sample's reading of the pinch is a guess rather than a measurement: one of its two
	ends is a carried position, or neither end can be put together at all. The same condition
	the gap below is logged on, and the one that hands the pinch to part_decide */
	int guessing = carried || !(thumb_usable && tip_usable);
	int path = carried ? HANDS_PATH_CARRIED : HANDS_PATH_MEASURED;

	if (!hand_tracked)
	{
		if (pinch->closed)
		{
			pinch->closed = 0;
			emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, 0.0f, HANDS_PINCH_OPEN_METERS,
				HANDS_PATH_HAND_LOST);
		}
		miss_ended(pinch, hand, finger, now, events, capacity, count);
		pinch->carried_since = 0.0;
		/* a hand ARKit is not reporting has no joints to judge anything by, so nothing this
		pinch recorded about its pair outlives the loss: the hand that comes back records its
		reference from what it is seen doing then, as tips_resolve rebuilds its carry */
		memset(pinch->part_reference_known, 0, sizeof(pinch->part_reference_known));
		pinch->parted = 0;
		return;
	}

	/* the gap's own log, on as it starts and off with its length as it ends: the distance below
	is a carried one, or there is none at all, and either way the player's finger is somewhere
	this file is guessing at. The gaps' lengths are the measurement to tune against */
	if (guessing)
	{
		if (pinch->carried_since == 0.0)
		{
			pinch->carried_since = now;
			emit(events, capacity, count, HANDS_EVENT_PINCH_MISS, hand, finger, 1, 0.0f, 0.0f,
				carried ? HANDS_PATH_CARRIED : HANDS_PATH_BLIND);
		}
	}
	else
	{
		miss_ended(pinch, hand, finger, now, events, capacity, count);
		pinch->carried_since = 0.0;
	}
	/* the pair behind the contact point decides a pinch whose tips this sample can only guess
	at, and it is asked ahead of the early return below so that a pinch with no reading at all
	is released by it too */
	if (!guessing)
		part_reference_record(pinch, pair);
	else if (part_decide(pinch, pair, hand, finger, events, capacity, count))
		return;
	/* nothing to measure and nothing to carry: the pinch keeps what it had rather than opening */
	if (!(thumb_usable && tip_usable))
		return;

	distance = distance3(thumb, tip);
	if (!pinch->closed && distance < HANDS_PINCH_CLOSE_METERS)
	{
		pinch->closed = 1;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 1, distance, HANDS_PINCH_CLOSE_METERS, path);
	}
	else if (pinch->closed && distance > HANDS_PINCH_OPEN_METERS)
	{
		pinch->closed = 0;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, distance, HANDS_PINCH_OPEN_METERS, path);
	}
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

/* the prayer sign's geometry gate alone: palm centers close and both hands' fingers pointed
up, independent of the palm normals' sign. *dot is the measured dot product whenever this
returns 1 (undefined otherwise, as prayer_ready already failed and the frames were never
built); separating this from the sign test is what lets hands_recognize log the dot it
measured even while the sign convention is wrong, rather than the gesture staying silent
until a device run has already settled it: a gate armed off joints that stay tracked can
report itself, where the sign cannot */
static int prayer_near(const struct host_spatial_hand_joints joints[2], float *dot)
{
	float center[2][3], normal[2][3], finger_direction[2][3];
	int hand;

	if (!prayer_ready(joints))
		return 0;
	for (hand = 0; hand < 2; hand++)
		palm_frame(&joints[hand], center[hand], normal[hand], finger_direction[hand]);
	*dot = dot3(normal[0], normal[1]);
	if (distance3(center[0], center[1]) > HANDS_PRAYER_DISTANCE_METERS)
		return 0;
	if (finger_direction[0][2] < HANDS_PRAYER_FINGERS_UP || finger_direction[1][2] < HANDS_PRAYER_FINGERS_UP)
		return 0;
	return 1;
}

/* one smooth turn's on or off transition, for the log: nothing while the turn stands as it
was. Snap mode's own on event comes from the snap branch, which is why this runs only in
smooth mode (hands_recognize) */
static void turn_transition(int *turning, int turning_now, int kind, int hand, int finger, float value,
	float threshold, struct hands_event *events, int capacity, int *count)
{
	if (*turning == turning_now)
		return;
	*turning = turning_now;
	emit(events, capacity, count, kind, hand, finger, turning_now, value, threshold, HANDS_PATH_NONE);
}

int hands_recognize(struct hands_state *state, const struct host_spatial_hand_joints joints[2], int aiming_hand,
	const float aim_forward[3], int turn_snap, float deadzone_degrees, double now,
	struct host_spatial_controls controls[2], struct hands_event *events, int capacity)
{
	int count = 0;
	int g = aiming_hand ? 1 : 0, o = !g;
	int hand, finger;
	/* each hand's loss as hand_lost decides it, for the zeroing at the end: a hand untracked
	but still inside its gap is neither tracked nor lost, and holds what it had */
	int lost[2];
	float aim_contribution = 0.0f, stick_x;
	/* what the turn last measured, for its own transition below: a turn the gun hand is
	not making measures 0, which is the reading its off event should carry */
	float aim_yaw = 0.0f;

	memset(controls, 0, 2 * sizeof(controls[0]));

	for (hand = 0; hand < 2; hand++)
	{
		const struct host_spatial_hand_joints *j = &joints[hand];
		/* this hand's tips for this sample, zeroed first: an untracked hand resolves nothing,
		and pinch_update is handed these whether it reads them or not */
		float tip[5][3];
		int usable[5], carried[5];

		memset(tip, 0, sizeof(tip));
		memset(usable, 0, sizeof(usable));
		memset(carried, 0, sizeof(carried));
		lost[hand] = hand_lost(state, hand, j->tracked, now);
		if (j->tracked)
			tips_resolve(state, hand, j, tip, usable, carried);
		else if (lost[hand])
			tips_forget(state, hand);
		/* a hand inside its gap is skipped outright: every pinch keeps the state it had, no
		event comes out, and the controls below are built from that kept state, so the gap
		holds the trigger and the walking stick as well as the pinches themselves */
		if (!j->tracked && !lost[hand])
			continue;

		for (finger = 0; finger < NUMBER_OF_HANDS_PINCHES; finger++)
		{
			int tip_index = finger + 1; /* tip[0] is the thumb */
			struct part_pair pair;

			part_pair_of(j, finger, &pair);
			pinch_update(&state->pinch[hand][finger], hand, finger, j->tracked, usable[0], tip[0],
				usable[tip_index], tip[tip_index], carried[0] || carried[tip_index], &pair, now, events,
				capacity, &count);
		}
	}

	/* the gun hand's buttons, and the off hand's stick and click. Both hands' little fingers
	stay unbound: switching and picking up a weapon is reserved for a reach-toward-the-ear
	gesture, not a pinch, so it is not this finger's to take */
	controls[g].trigger = state->pinch[g][HANDS_PINCH_INDEX].closed ? 1.0f : 0.0f;
	controls[g].face_low = state->pinch[g][HANDS_PINCH_RING].closed ? 1 : 0;
	controls[g].face_high = state->pinch[g][HANDS_PINCH_MIDDLE].closed ? 1 : 0;
	if (state->pinch[o][HANDS_PINCH_INDEX].closed)
		controls[o].stick_y = 1.0f;
	else if (state->pinch[o][HANDS_PINCH_MIDDLE].closed)
		controls[o].stick_y = -1.0f;
	controls[o].stick_click = state->pinch[o][HANDS_PINCH_RING].closed ? 1 : 0;

	/* an untracked gun hand has no aim to turn from, and nothing below cleans up after one:
	the zeroing at the end of this function spares a hand that is only inside its gap, so this
	gate is the one thing keeping a stale barrel out of the turn. It could not undo an event
	already emitted or an armed/pulse state already set from whatever aim_forward the caller
	passed either, which for an untracked hand is a zero vector whose yaw reads 0. Gating the
	whole computation on joints[g].tracked rather than on the gap keeps the aim turn silent and
	its state untouched for as long as ARKit is not reporting the hand: a blink holds that
	hand's pinches (hand_lost), never the geometry its joints would give */
	if (joints[g].tracked)
	{
		float yaw, excess, turn_sign;

		yaw = atan2f(aim_forward[1], aim_forward[0]) * (180.0f / (float)M_PI);
		excess = fabsf(yaw) - deadzone_degrees;
		turn_sign = yaw < 0.0f ? -1.0f : 1.0f;
		aim_yaw = yaw;

		if (turn_snap)
		{
			if (excess < 0.0f)
				state->aim_turn_armed = 1;
			else if (excess > 0.0f && state->aim_turn_armed)
			{
				state->pulse_until = now + HANDS_PULSE_SECONDS;
				state->pulse_x = -turn_sign;
				state->pulse_kind = HANDS_EVENT_AIM_TURN;
				state->aim_turn_armed = 0;
				emit(events, capacity, &count, HANDS_EVENT_AIM_TURN, g, 0, 1, yaw, deadzone_degrees,
					HANDS_PATH_NONE);
			}
		}
		else
			aim_contribution = -turn_sign *
				clampf(excess / HANDS_AIM_TURN_RAMP_DEGREES, 0.0f, 1.0f);
	}

	/* snap mode is pulse-only (no smooth fallback); smooth mode is the aim turn's own
	contribution; either way a live pulse overrides it */
	stick_x = turn_snap ? 0.0f : clampf(aim_contribution, -1.0f, 1.0f);
	if (now < state->pulse_until)
		stick_x = state->pulse_x;
	controls[g].stick_x = stick_x;
	/* the continuous turn reports itself the way the snap branch above reports its pulse: on as
	the contribution first becomes nonzero, off as it falls back to zero, with what it measured
	against the deadzone it crossed. Without this the log says nothing at all about smooth
	turning, which is what the deadzone has to be tuned against */
	if (!turn_snap)
		turn_transition(&state->aim_turning, aim_contribution != 0.0f, HANDS_EVENT_AIM_TURN, g, 0, aim_yaw,
			deadzone_degrees, events, capacity, &count);
	/* a pulse's own end, so the log carries the width the caller's sampler gave it rather than
	only the instant it began. pulse_kind is the aim turn, the only source that sets a pulse */
	if (state->pulse_until > 0.0 && now >= state->pulse_until)
	{
		emit(events, capacity, &count, state->pulse_kind, g, 0, 0, state->pulse_x, (float)HANDS_PULSE_SECONDS,
			HANDS_PATH_NONE);
		state->pulse_until = 0.0;
	}

	/* the prayer sign: both hands held together fires the Start pulse once per dwell. The
	geometry gate (prayer_near) is logged on its own on/off transition with the dot it
	measured against HANDS_PRAYER_OPPOSED_DOT: gating that log on the sign itself, like
	prayer_holds used to, would mean a wrong sign convention never logs anything, which is
	exactly what the device measurement that settles the sign has to read */
	{
		float dot = 0.0f;
		int near = prayer_near(joints, &dot);

		if (near != state->prayer_near)
		{
			state->prayer_near = near;
			emit(events, capacity, &count, HANDS_EVENT_PRAYER, 0, 0, near, dot, HANDS_PRAYER_OPPOSED_DOT,
				HANDS_PATH_NONE);
		}
		if (near && dot <= HANDS_PRAYER_OPPOSED_DOT)
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
				emit(events, capacity, &count, HANDS_EVENT_START, g, 0, 1, 1.0f, (float)HANDS_PULSE_SECONDS,
					HANDS_PATH_NONE);
			}
		}
		else
		{
			state->prayer_since = 0.0;
			state->prayer_fired = 0;
		}
	}
	controls[g].menu = now < state->start_until;
	/* and the Start pulse's end, for the same reason */
	if (state->start_until > 0.0 && now >= state->start_until)
	{
		emit(events, capacity, &count, HANDS_EVENT_START, g, 0, 0, 1.0f, (float)HANDS_PULSE_SECONDS,
			HANDS_PATH_NONE);
		state->start_until = 0.0;
	}

	/* a lost hand (hand_lost: untracked past its gap) zeros every control of that hand, the
	same sample, regardless of anything computed above. A hand still inside its gap is not
	zeroed, which is what carries a held trigger or a walking stick through a blink. The Start
	pulse is the one exception to the zeroing: the prayer sign is both hands coming together,
	and dropping them is the natural thing to do next, which would otherwise eat the pulse the
	sign just earned inside its own 100 ms window */
	{
		int start_pulse = controls[g].menu;

		if (lost[0])
			memset(&controls[0], 0, sizeof(controls[0]));
		if (lost[1])
			memset(&controls[1], 0, sizeof(controls[1]));
		controls[g].menu = start_pulse;
	}

	/* emit() only ever writes within capacity, but count keeps counting past it; clamp the
	return so a caller's for (i = 0; i < count; i++) against events[capacity] is always in
	bounds, even when a sample emits more state changes than capacity allows (both hands
	lost at once, every pinch closed and every one of them in a carried gap, reaches 16) */
	return count < capacity ? count : capacity;
}
