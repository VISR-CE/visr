/* The gesture recognizer (host_spatial_hands.h, declared): pinch hysteresis
over fingertips carried in the palm's frame through ARKit's dropouts, with its
two thresholds scaled off a ruler measured from each of the player's own
fingers rather than fixed in meters, the
check on the joints behind a pinch that releases it when the fingers part
rather than when the hidden fingertip is found again, the gun and off hand's
button and stick mapping, the aim turn and the arm's elevation, and the
prayer sign's dwell and Start pulse. Plain C over the reduced joint facts only, so
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

/* one reading of a finger's own ruler, folded into that finger's window
(host_spatial_hands.h's struct hands_pinch, its ruler_sample). Only a reading that could be a
finger gets in: outside HANDS_RULER_MINIMUM_METERS to HANDS_RULER_MAXIMUM_METERS the number is
a tracking glitch, and it is dropped rather than clamped to the nearer end, since a boundary
value folded in still drags the window toward the boundary where a dropped one leaves the
window exactly as it was. A window that only ever sees glitches therefore stays empty and
ruler_meters below answers with the default, which is what the zero-ruler case needs: no
threshold anywhere in this file ever becomes 0 */
static void ruler_push(struct hands_pinch *pinch, float meters)
{
	if (!(meters >= HANDS_RULER_MINIMUM_METERS && meters <= HANDS_RULER_MAXIMUM_METERS))
		return;
	pinch->ruler_sample[pinch->ruler_next] = meters;
	pinch->ruler_next = (pinch->ruler_next + 1) % HANDS_RULER_SAMPLES;
	if (pinch->ruler_count < HANDS_RULER_SAMPLES)
		pinch->ruler_count++;
}

/* this finger's ruler as its window now stands: the median of its readings, or
HANDS_RULER_DEFAULT_METERS while there are too few of them to trust one
(HANDS_RULER_MINIMUM_SAMPLES). The return is never 0 and never outside the plausible range,
since every value in the window passed ruler_push's test and the default sits inside it too,
so neither threshold scaled off this can collapse or run away */
static float ruler_meters(const struct hands_pinch *pinch)
{
	float sorted[HANDS_RULER_SAMPLES];
	int count = pinch->ruler_count, i, j;

	if (count < HANDS_RULER_MINIMUM_SAMPLES)
		return HANDS_RULER_DEFAULT_METERS;
	/* the valid readings are the first count of the ring however far ruler_next has wrapped:
	a full window is all of it and a partial one is [0, count), so one copy covers both, and
	the order within it does not matter to a median */
	memcpy(sorted, pinch->ruler_sample, (size_t)count * sizeof(sorted[0]));
	/* an insertion sort over at most HANDS_RULER_SAMPLES values, once per pinch per sample:
	at this size the cheapest sort to read is the right one, and nothing here sits on a path
	where a better asymptote would be measurable */
	for (i = 1; i < count; i++)
	{
		float value = sorted[i];

		for (j = i; j > 0 && sorted[j - 1] > value; j--)
			sorted[j] = sorted[j - 1];
		sorted[j] = value;
	}
	return sorted[count / 2];
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

/* debug.hand_distance_log alone (host_spatial_hands.m's periodic log): the same tips_resolve
every pinch already reads this sample, read again here for the four raw distances rather
than a decision. Idempotent with hands_recognize's own call into it for this sample: given
the same state and the same joints, an ARKit-tracked tip encodes the same palm-frame
position it already did and a carried one decodes the same position back, so calling this
after hands_recognize has run writes nothing new to state->tip_local or state->tip_known.
palm_width and index_distal need no carry and no tips_resolve at all, since the thumb never
hides the index metacarpal or the index finger's own intermediate tip the way it hides a
fingertip: they are read straight off joints, 0 and not-known where ARKit isn't reporting
the joint either one needs this sample */
void hands_tip_distances(struct hands_state *state, int hand, const struct host_spatial_hand_joints *joints,
	struct hands_tip_distances *out)
{
	float tip[5][3];
	int usable[5], carried[5];
	int finger;

	memset(tip, 0, sizeof(tip));
	memset(usable, 0, sizeof(usable));
	memset(carried, 0, sizeof(carried));
	if (joints->tracked)
		tips_resolve(state, hand, joints, tip, usable, carried);
	for (finger = 0; finger < NUMBER_OF_HANDS_PINCHES; finger++)
	{
		int tip_index = finger + 1; /* tip[0] is the thumb, as tips_resolve's own array is */

		if (usable[0] && usable[tip_index])
		{
			out->distance[finger] = distance3(tip[0], tip[tip_index]);
			out->path[finger] = (carried[0] || carried[tip_index]) ?
				HANDS_DISTANCE_CARRIED : HANDS_DISTANCE_MEASURED;
		}
		else
		{
			out->distance[finger] = 0.0f;
			out->path[finger] = HANDS_DISTANCE_UNAVAILABLE;
		}
	}
	out->palm_width_known = joints->metacarpal_tracked[0] && joints->metacarpal_tracked[1];
	out->palm_width = out->palm_width_known ? distance3(joints->metacarpal[0], joints->metacarpal[1]) : 0.0f;
	/* finger 1 of each chain is the index finger, as tip[] and intermediate_tip[] both order
	it (thumb, index, middle, ring, little: this file's part_pair_of comment) */
	out->index_distal_known = joints->intermediate_tip_tracked[1] && joints->tip_tracked[1];
	out->index_distal = out->index_distal_known ?
		distance3(joints->intermediate_tip[1], joints->tip[1]) : 0.0f;
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
	/* the ladder in the order host_spatial_hands.h argues for it, as a table rather than six
	cases: the finger's end stepping back down the finger, the thumb's end stepping back within
	each of those, so a level's two ends read off one line and adding a level is adding a row */
	const float *finger_end[NUMBER_OF_HANDS_PART_LEVELS] = {
		joints->intermediate_tip[chain], joints->intermediate_tip[chain],
		joints->intermediate_base[chain], joints->intermediate_base[chain],
		joints->knuckle[chain], joints->knuckle[chain]
	};
	int finger_tracked[NUMBER_OF_HANDS_PART_LEVELS] = {
		joints->intermediate_tip_tracked[chain], joints->intermediate_tip_tracked[chain],
		joints->intermediate_base_tracked[chain], joints->intermediate_base_tracked[chain],
		joints->knuckle_tracked[chain], joints->knuckle_tracked[chain]
	};
	const float *thumb_end[NUMBER_OF_HANDS_PART_LEVELS] = {
		joints->intermediate_tip[0], joints->intermediate_base[0],
		joints->intermediate_tip[0], joints->intermediate_base[0],
		joints->intermediate_tip[0], joints->intermediate_base[0]
	};
	int thumb_tracked[NUMBER_OF_HANDS_PART_LEVELS] = {
		joints->intermediate_tip_tracked[0], joints->intermediate_base_tracked[0],
		joints->intermediate_tip_tracked[0], joints->intermediate_base_tracked[0],
		joints->intermediate_tip_tracked[0], joints->intermediate_base_tracked[0]
	};
	int level;

	for (level = 0; level < NUMBER_OF_HANDS_PART_LEVELS; level++)
	{
		pair->known[level] = finger_tracked[level] && thumb_tracked[level];
		pair->distance[level] = pair->known[level] ?
			distance3(finger_end[level], thumb_end[level]) : 0.0f;
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
	pinch->part_over = 0;
}

/* the first level of the ladder this sample can both see and compare against, and how far
that level has grown from its reference. Returns the level, or -1 where no level has both,
which is the one case the check cannot speak at all. Separated out because two callers want
the same number: part_decide, which acts on it, and pinch_update, which reports it on every
release the fingertips themselves decided so that HANDS_PART_MARGIN_METERS can be measured
rather than argued (host_spatial_hands.h's HANDS_EVENT_PART_GROWTH) */
static int part_growth(const struct hands_pinch *pinch, const struct part_pair *pair, float *growth)
{
	int level;

	for (level = 0; level < NUMBER_OF_HANDS_PART_LEVELS; level++)
		if (pair->known[level] && pinch->part_reference_known[level])
		{
			*growth = pair->distance[level] - pinch->part_reference[level];
			return level;
		}
	*growth = 0.0f;
	return -1;
}

/* the end of a hold, for the log: how long this pinch had been closed, in seconds, with the
path that released it (host_spatial_hands.h's HANDS_EVENT_PINCH_HELD). Every release calls
it, right after its own pinch-off line, so one line per release comes out whichever of the
three readings decided it. Silent unless a close was recorded, which keeps a pinch found
closed in a freshly zeroed state out of the log rather than dating it from 0.

Instrumentation and nothing else: it clears closed_since, touches no other field, and no rule
in this file reads the duration. Three rounds of work have each tuned a proxy for a prompt
release while the thing the player reports, a tap that lasts long enough to charge instead of
fire, went unmeasured; the next change to those rules should be a number read off these
lines */
static void held_ended(struct hands_pinch *pinch, int hand, int finger, double now, int path,
	struct hands_event *events, int capacity, int *count)
{
	if (pinch->closed_since <= 0.0)
		return;
	emit(events, capacity, count, HANDS_EVENT_PINCH_HELD, hand, finger, 0, (float)(now - pinch->closed_since),
		0.0f, path);
	pinch->closed_since = 0.0;
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
player holds it (hands_probe.c's pinch_hold_to_fire_never_releases): the confirmation below
counts consecutive samples past the margin and resets on the first one inside it, so nothing
here accumulates with the length of a hold.

Which joints, and how far they have to grow, are the two things a device session corrected.
The first version watched the finger's middle knuckle, which barely translates when a pinch
opens, and asked for 12 mm of growth from it: over a whole session it released one pinch,
and every other release waited for the hidden fingertip exactly as it had before the check
existed. Both halves of that are fixed here, the joints in part_pair_of and the margin in
host_spatial_hands.h, and pinch_update now reports the growth at every release the
fingertips decided so the margin is measured next time rather than argued again */
static int part_decide(struct hands_pinch *pinch, const struct part_pair *pair, int hand, int finger,
	double now, struct hands_event *events, int capacity, int *count)
{
	float growth;

	/* no level this sample can both see and compare against: the carry's answer stands, which
	is a closed pinch holding, exactly as before this check existed. A release the pair has
	already decided stands too, since the pair going missing is not the fingers coming back
	together */
	if (part_growth(pinch, pair, &growth) < 0)
		return pinch->parted;
	/* a band, as this file's every other threshold is: past the margin the fingers are
	apart, and only back under half of it is a carried position trusted to close the pinch
	again. Without the lower edge a pair sitting at the margin would release and re-close
	sample after sample, turning one held pinch into a stream of shots.

	Past the margin is also counted rather than acted on at once (HANDS_PART_CONFIRM_SAMPLES):
	the band's own shape is what makes one noisy sample expensive, since parted does not clear
	again until the growth falls under half the margin, which a hand that is still pinching
	will not do. A real parting grows further on the sample after it starts and confirms
	immediately; a spike has to happen twice running */
	if (growth > HANDS_PART_MARGIN_METERS)
	{
		if (pinch->part_over < HANDS_PART_CONFIRM_SAMPLES)
			pinch->part_over++;
		if (pinch->part_over >= HANDS_PART_CONFIRM_SAMPLES)
			pinch->parted = 1;
	}
	else
	{
		pinch->part_over = 0;
		if (growth < HANDS_PART_MARGIN_METERS / 2.0f)
			pinch->parted = 0;
	}
	if (pinch->parted && pinch->closed)
	{
		pinch->closed = 0;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, growth,
			HANDS_PART_MARGIN_METERS, HANDS_PATH_PARTED);
		held_ended(pinch, hand, finger, now, HANDS_PATH_PARTED, events, capacity, count);
	}
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

/* close_meters and open_meters are this finger's own band for this sample, already scaled off
its ruler by hands_recognize (HANDS_PINCH_CLOSE_RULER_FRACTION in host_spatial_hands.h): passed
in rather than read from a macro here so that the two numbers are the caller's one decision and
every event this function emits reports the threshold the decision was actually made against,
which is what debug.hand_log puts on device */
static void pinch_update(struct hands_pinch *pinch, int hand, int finger, int hand_tracked, int thumb_usable,
	const float thumb[3], int tip_usable, const float tip[3], int carried, const struct part_pair *pair,
	float close_meters, float open_meters,
	double now, struct hands_event *events, int capacity, int *count)
{
	float distance;
	/* this sample's reading of the pinch is a guess rather than a measurement: one of its two
	ends is a carried position, or neither end can be put together at all. The same condition
	the gap below is logged on, and the one that hands the pinch to part_decide */
	int guessing = carried || !(thumb_usable && tip_usable);
	int path = carried ? HANDS_PATH_CARRIED : HANDS_PATH_MEASURED;
	/* the pair's growth as this sample arrived, and whether a gap was running when it did:
	both are read before anything below moves them, since part_reference_record resets the
	reference the growth is measured from and miss_ended clears the gap. They are what the
	measured release at the end of this function reports, and the measurement that settles
	HANDS_PART_MARGIN_METERS (host_spatial_hands.h's HANDS_EVENT_PART_GROWTH) */
	float arrival_growth = 0.0f;
	int arrival_level, arrival_in_gap;

	if (!hand_tracked)
	{
		if (pinch->closed)
		{
			pinch->closed = 0;
			emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, 0.0f, open_meters,
				HANDS_PATH_HAND_LOST);
			held_ended(pinch, hand, finger, now, HANDS_PATH_HAND_LOST, events, capacity, count);
		}
		miss_ended(pinch, hand, finger, now, events, capacity, count);
		pinch->carried_since = 0.0;
		/* a hand ARKit is not reporting has no joints to judge anything by, so nothing this
		pinch recorded about its pair outlives the loss: the hand that comes back records its
		reference from what it is seen doing then, as tips_resolve rebuilds its carry */
		memset(pinch->part_reference_known, 0, sizeof(pinch->part_reference_known));
		pinch->parted = 0;
		pinch->part_over = 0;
		return;
	}

	arrival_level = part_growth(pinch, pair, &arrival_growth);
	arrival_in_gap = pinch->carried_since > 0.0;

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
	else if (part_decide(pinch, pair, hand, finger, now, events, capacity, count))
		return;
	/* nothing to measure and nothing to carry: the pinch keeps what it had rather than opening */
	if (!(thumb_usable && tip_usable))
		return;

	distance = distance3(thumb, tip);
	if (!pinch->closed && distance < close_meters)
	{
		pinch->closed = 1;
		/* the only place a hold's clock starts, so the release that ends it reports the whole of
		it however it is ended (held_ended) */
		pinch->closed_since = now;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 1, distance, close_meters, path);
	}
	else if (pinch->closed && distance > open_meters)
	{
		pinch->closed = 0;
		emit(events, capacity, count, HANDS_EVENT_PINCH, hand, finger, 0, distance, open_meters, path);
		held_ended(pinch, hand, finger, now, path, events, capacity, count);
		/* and what the pair behind this pinch had grown by at the instant the tips themselves
		released it, which is the margin's own measurement: a release decided here rather than
		by part_decide is a release the part check did not catch, so the growths on these lines
		are the distribution HANDS_PART_MARGIN_METERS has to sit at the bottom of. Reported
		whatever that growth was, including the growths well under the margin, since those are
		the ones worth seeing. Only on a measured release: a carried one is a guess at both
		ends, so the growth beside it would describe nothing */
		if (!guessing)
			emit(events, capacity, count, HANDS_EVENT_PART_GROWTH, hand, finger, arrival_in_gap,
				arrival_growth, HANDS_PART_MARGIN_METERS,
				arrival_level >= 0 ? HANDS_PATH_MEASURED : HANDS_PATH_BLIND);
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

/* one continuous aim axis's on or off transition, for the log: nothing while the axis stands
as it was. The smooth turn is one caller and the arm's elevation the other, each with its own
flag in hands_state. Snap mode's turn reports itself from the snap branch instead, which is
why the turn's call runs only in smooth mode; the elevation has no snap mode and so always
calls (hands_recognize) */
static void turn_transition(int *turning, int turning_now, int kind, int hand, int finger, float value,
	float threshold, struct hands_event *events, int capacity, int *count)
{
	if (*turning == turning_now)
		return;
	*turning = turning_now;
	emit(events, capacity, count, kind, hand, finger, turning_now, value, threshold, HANDS_PATH_NONE);
}

int hands_recognize(struct hands_state *state, const struct host_spatial_hand_joints joints[2], int aiming_hand,
	const float aim_forward[3], int turn_snap, float deadzone_degrees, float pitch_offset_degrees, double now,
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
	/* and the same pair for the arm's elevation, read off the same aim forward: an untracked
	gun hand leaves both at 0, as it leaves the turn's at 0 */
	float elevation_contribution = 0.0f, aim_elevation = 0.0f;

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
			float ruler;

			/* this finger's own ruler before anything reads a threshold off it, so a sample
			that can measure the segment scales that same sample's band. Read straight off
			joints and only where ARKit reports both of the segment's ends: the carry in
			tips_resolve answers where the finger probably still is, and a probability has no
			business setting the threshold it is then judged against. Each finger gets its own
			because each finger is a different length: the middle, ring and little fingers'
			distal segments are shorter than the index's, and on device their resting thumb
			distances run near 80 mm where the index's runs near 25, so one ruler for the hand
			would scale three of the four fingers by a segment that is not theirs */
			if (j->tracked && j->intermediate_tip_tracked[tip_index] && j->tip_tracked[tip_index])
				ruler_push(&state->pinch[hand][finger],
					distance3(j->intermediate_tip[tip_index], j->tip[tip_index]));
			ruler = ruler_meters(&state->pinch[hand][finger]);
			part_pair_of(j, finger, &pair);
			pinch_update(&state->pinch[hand][finger], hand, finger, j->tracked, usable[0], tip[0],
				usable[tip_index], tip[tip_index], carried[0] || carried[tip_index], &pair,
				HANDS_PINCH_CLOSE_RULER_FRACTION * ruler, HANDS_PINCH_OPEN_RULER_FRACTION * ruler,
				now, events, capacity, &count);
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
	gate is the one thing keeping a stale barrel out of the turn and the elevation both. It
	could not undo an event already emitted or an armed/pulse state already set from whatever
	aim_forward the caller passed either, which for an untracked hand is a zero vector whose
	yaw and elevation both read 0. Gating the whole computation on joints[g].tracked rather
	than on the gap keeps both axes silent and their state untouched for as long as ARKit is
	not reporting the hand: a blink holds that hand's pinches (hand_lost), never the geometry
	its joints would give */
	if (joints[g].tracked)
	{
		float yaw, excess, turn_sign;
		float elevation, elevation_excess, elevation_sign;

		yaw = atan2f(aim_forward[1], aim_forward[0]) * (180.0f / (float)M_PI);
		excess = fabsf(yaw) - deadzone_degrees;
		turn_sign = yaw < 0.0f ? -1.0f : 1.0f;
		aim_yaw = yaw;

		/* the arm's elevation off the same barrel the yaw comes from: how far the aim rises out
		of the room's horizontal plane, which is the aim forward's up component against the
		length of its horizontal part (aim_forward's index 2 is up, as host_spatial_pose.h's
		host_spatial_room_to_game leaves it). Signed as the yaw is, with the same deadzone and
		the same ramp, so the two axes of the one stick feel alike under the one arm.
		The sign is positive upward and that is the whole point of it: controls[g].stick_y
		reaches the game as the right stick's vertical axis
		(host_spatial_gamepad.c's host_spatial_gamepad_map fills thumb[3]), which
		input_abstraction.c's input_abstraction_update turns into input_state->pitch, and
		player_control.c sets _player_control_look_relative_up_bit for a positive pitch. So
		raising the arm raises the gun, and lowering it lowers it, with no sign flip anywhere
		between.

		input.aim_pitch_offset is subtracted before the deadzone and the ramp ever see it: it
		is the same downward correction controller_aim_game.c reads into pitch_offset_degrees
		and hands to controller_aim.c's controller_location_to_world, which rotates the aim
		forward about the location's left axis (forward toward -up, positive down) before the
		guest ever draws a sightline from it. This file has only the forward component, not
		the hand's up or left, but it does not need them: aim_up (host_spatial_hands.m) builds
		that up as the room's up squared off against this same forward, which puts forward and
		up in the one vertical plane that also holds the room's vertical and this forward's
		heading. Turning forward toward -up by an angle theta inside that plane is turning its
		elevation from phi to phi - theta while its horizontal direction keeps the same heading,
		scaled by cos(phi - theta) rather than flipped, for any arm pose this recognizer ever
		sees. So subtracting degrees from the elevation this file already measures is the same
		rotation controller_location_to_world does, without carrying the up vector through this
		call. Nothing here touches yaw: it stays off the raw aim_forward, so the trim never
		moves the turn */
		elevation = atan2f(aim_forward[2], sqrtf(aim_forward[0] * aim_forward[0] +
			aim_forward[1] * aim_forward[1])) * (180.0f / (float)M_PI) - pitch_offset_degrees;
		elevation_excess = fabsf(elevation) - deadzone_degrees;
		elevation_sign = elevation < 0.0f ? -1.0f : 1.0f;
		aim_elevation = elevation;
		elevation_contribution = elevation_sign *
			clampf(elevation_excess / HANDS_AIM_TURN_RAMP_DEGREES, 0.0f, 1.0f);

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
	/* the arm's elevation on the same stick's vertical axis, and no seat test anywhere in this
	file to gate it: the game's own rule already decides whether the axis means anything.
	stereo.c's halo_stereo_stick_look drops the right stick's pitch outright while
	head_tracking() is true, which is a player on foot in head-tracked stereo, where the head
	supplies pitch and this axis is discarded unread. It returns before that line when
	head_tracking() is false, and a seat whose camera is third person (a Shade's or a Warthog's
	turret) is exactly that case, so there the stick's pitch survives and elevates the gun.
	A turret has no other way to aim up or down from the hands, since the head is not driving
	it, so the axis is filled always and the game gates it.

	Written unconditionally, unlike stick_x above: snap mode zeroes the turn because a snap is
	a discrete step of yaw, but there is no such thing as a snapped elevation, and a player who
	picked snap turning would otherwise have no vertical in a turret at all. The pulse override
	stays on stick_x, which is the only axis a snap ever writes */
	controls[g].stick_y = clampf(elevation_contribution, -1.0f, 1.0f);
	/* and it reports itself the way the smooth turn does, on both turn settings for the same
	reason it is computed on both */
	turn_transition(&state->aim_elevating, elevation_contribution != 0.0f, HANDS_EVENT_AIM_ELEVATION, g, 0,
		aim_elevation, deadzone_degrees, events, capacity, &count);
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
	bounds, even when a sample emits more lines than capacity allows. The worst sample is
	eight pinches each ending a carried gap, releasing from their own fingertips, and
	reporting both the pair's growth for that release and the length of the hold it ended,
	which reaches 32: what host_spatial_hands.m sizes its buffer to, since the growth and the
	hold are the lines worth keeping */
	return count < capacity ? count : capacity;
}
