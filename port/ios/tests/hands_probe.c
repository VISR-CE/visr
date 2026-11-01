/* The gesture recognizer (port/ios/host/host_spatial_hands.c, included):
pinch hysteresis and the fingertip carried in the palm's frame through a
dropout, the release a pinch gets from the joints behind it once the fingers
part while that fingertip is still hidden (and that holding the pinch never
earns one), the gun and off hand's button and stick mapping, the aim turn
(smooth and snap), the prayer sign's dwell and Start pulse, the whole-hand
gap a blink is held through, and that a hand untracked past that gap opens
every one of its pinches and zeros every one of its own controls. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../host/host_spatial_hands.c"

static int failures;

static void check(int condition, const char *what)
{
	if (!condition)
	{
		printf("FAILED: %s\n", what);
		failures++;
	}
}

static int near(float value, float wanted, float tolerance)
{
	return fabsf(value - wanted) <= tolerance;
}

/* a tracked, open hand: the thumb tip at the origin, the other four tips
5 cm away along up, a neutral wrist and an arbitrary open-hand metacarpal
pose. Tests move one tip, the wrist or the metacarpals to build the case
they need. hand (0 or 1, joints[]'s own index) sets the wrist and
metacarpals a meter apart on the room's left axis: two default hands are
never close enough for the prayer sign's geometry gate (host_spatial_hands.c's
prayer_near) to fire by accident in a test that isn't about it, and a test
that cares about either hand's absolute wrist position already overwrites it
before using it, so the offset never reaches an assertion */
static struct host_spatial_hand_joints make_hand(int hand)
{
	struct host_spatial_hand_joints j;
	float wrist_y = hand ? 1.0f : 0.0f;
	int i;

	memset(&j, 0, sizeof(j));
	j.tracked = 1;
	j.forearm_tracked = 1;
	j.wrist[1] = wrist_y;
	for (i = 0; i < 5; i++)
	{
		j.tip_tracked[i] = 1;
		j.tip[i][2] = (i == 0) ? 0.0f : 0.05f;
		/* the chain walking back from each tip: the thumb's toward the wrist, each finger's
		down the finger. What they put the part check's pair at (host_spatial_hands.c's
		part_pair_of) is 3.0 cm for the thumb's intermediate tip against a finger's
		intermediate base and 5.0 cm for its intermediate base, which set_part_pair below
		moves and the tests read against HANDS_PART_MARGIN_METERS */
		j.intermediate_tip_tracked[i] = 1;
		j.intermediate_base_tracked[i] = 1;
		j.intermediate_tip[i][2] = (i == 0) ? -0.020f : 0.030f;
		j.intermediate_base[i][2] = (i == 0) ? -0.040f : 0.010f;
	}
	j.metacarpal_tracked[0] = j.metacarpal_tracked[1] = 1;
	j.metacarpal[0][0] = 0.03f;
	j.metacarpal[0][1] = wrist_y;
	j.metacarpal[0][2] = 0.05f;
	j.metacarpal[1][0] = -0.03f;
	j.metacarpal[1][1] = wrist_y;
	j.metacarpal[1][2] = 0.05f;
	return j;
}

/* one hand moved as a rigid body: every joint position turned roll_degrees about the game's
forward axis (x forward, y left, z up, as host_spatial_pose.h's host_spatial_room_to_game
leaves them) and then offset. A tip carried through a dropout is stored in the palm's frame
(host_spatial_hands.c's palm_basis), so it has to follow the hand through a move like this;
a tip kept in the room's axes instead would be left tens of centimeters behind */
static void move_hand(struct host_spatial_hand_joints *j, float roll_degrees, const float offset[3])
{
	float c = cosf(roll_degrees * (float)M_PI / 180.0f), s = sinf(roll_degrees * (float)M_PI / 180.0f);
	float *points[19];
	int i, count = 0;

	points[count++] = j->wrist;
	points[count++] = j->forearm_arm;
	for (i = 0; i < 5; i++)
	{
		points[count++] = j->tip[i];
		/* the part check's own joints move with the rest or the hold-to-fire case below
		proves nothing: a pair left behind while every other joint turns keeps its
		separation for the wrong reason */
		points[count++] = j->intermediate_tip[i];
		points[count++] = j->intermediate_base[i];
	}
	for (i = 0; i < 2; i++)
		points[count++] = j->metacarpal[i];
	for (i = 0; i < count; i++)
	{
		float y = points[i][1], z = points[i][2];

		points[i][0] += offset[0];
		points[i][1] = y * c - z * s + offset[1];
		points[i][2] = y * s + z * c + offset[2];
	}
}

/* one pinch's still-visible pair set to a given separation, by moving only the finger's
intermediate base: the joints host_spatial_hands.c's part_pair_of reads, and none of the
fingertips. That is the whole case the check is for, since on device the tips are what the
pinching thumb hides. make_hand leaves the pair at 3.0 cm, so 5.0 cm is 2.0 cm of parting */
static void set_part_pair(struct host_spatial_hand_joints *j, int finger, float meters)
{
	j->intermediate_base[finger + 1][0] = j->intermediate_tip[0][0];
	j->intermediate_base[finger + 1][1] = j->intermediate_tip[0][1];
	j->intermediate_base[finger + 1][2] = j->intermediate_tip[0][2] + meters;
}

/* every field of a controls struct is zero */
static int controls_all_zero(const struct host_spatial_controls *controls)
{
	return controls->trigger == 0.0f && controls->grip == 0.0f && controls->face_low == 0 &&
		controls->face_high == 0 && controls->stick_click == 0 && controls->menu == 0 &&
		controls->stick_x == 0.0f && controls->stick_y == 0.0f;
}

/* pinch hysteresis: closes below CLOSE, stays through the gap to OPEN, opens above OPEN, each
transition logging one event with the distance and the threshold that decided it */
static void pinch_hysteresis(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);

	joints[1].tip[1][2] = 0.010f; /* the index tip at 1.0 cm: closes */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch: 1.0 cm closes the trigger");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].hand == 1 &&
		events[0].finger == HANDS_PINCH_INDEX && events[0].on == 1, "pinch: one pinch-on event for the index");

	joints[1].tip[1][2] = 0.024f; /* 2.4 cm: inside the hysteresis band, no change */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch: 2.4 cm still closed (hysteresis)");
	check(count == 0, "pinch: no event inside the hysteresis band");

	joints[1].tip[1][2] = 0.040f; /* 4.0 cm: opens */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch: 4.0 cm opens the trigger");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0, "pinch: one pinch-off event");
	check(controls[0].menu == 0, "pinch: the off hand's menu is never set");
}

/* a pinch closed when ARKit drops the fingertip stays closed for as long as the dropout lasts,
with the hand itself still tracked: the tip is carried in the palm's frame
(host_spatial_hands.c's tips_resolve), the gap shows in the log as a miss, and no pinch event
comes out of it. On device one finger's dropouts ran from 0.07 s to 21 s in a single session */
static void pinch_survives_a_tip_dropout(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	double t;
	int count, i, pinch_events = 0, opened = 0;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* close the index pinch at t = 1.0 */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip dropout: starts closed");

	joints[1].tip_tracked[1] = 0; /* the index tip drops out at t = 1.05 */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip dropout: still closed as the tip drops out");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX, "tip dropout: the gap is logged as a miss, not as a pinch");

	for (t = 1.10; t <= 4.05 + 1e-9; t += 0.05) /* 3.0 s of it */
	{
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
		for (i = 0; i < count; i++)
			if (events[i].kind == HANDS_EVENT_PINCH)
				pinch_events++;
		if (controls[1].trigger != 1.0f)
			opened++;
	}
	check(opened == 0, "tip dropout: closed through 3.0 s of dropout");
	check(pinch_events == 0, "tip dropout: no pinch event during the gap");

	joints[1].tip_tracked[1] = 1; /* the tip returns, still pinched */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 4.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip dropout: still closed as the tip returns");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		near(events[0].value, 3.05f, 0.01f), "tip dropout: the gap's length is logged as it ends");
}

/* the same dropout with the hand moving through it: the carried tip is stored in the palm's
frame, so a hand translated and turned while its fingertip is invisible carries that tip along
and the pinch stays closed */
static void pinch_carry_follows_the_palm(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	const float across[3] = { 0.30f, -0.20f, 0.10f }, back[3] = { -0.15f, 0.25f, -0.05f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: starts closed");

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: closed as the tip drops out");

	move_hand(&joints[1], 40.0f, across);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.5, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: closed with the hand turned 40 degrees and moved 0.37 m");
	check(count == 0, "carried tip: the move itself says nothing");

	move_hand(&joints[1], -70.0f, back);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 2.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: closed again with the hand turned back the other way");
	check(count == 0, "carried tip: the second move says nothing either");

	joints[1].tip_tracked[1] = 1; /* the tip is seen again where the moved palm says it is */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 2.5, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: the real reading agrees, still closed");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0,
		"carried tip: only the gap's end is logged");
}

/* a tip lost while the pinch is open and found closed fires on the sample it returns: nothing
stands between the reading and the press, since a pinch no longer has to be seen apart before
it may close */
static void pinch_closes_when_the_tip_returns(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tip returns closed: starts open, the tips 5 cm from the thumb");

	joints[1].tip_tracked[1] = 0;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tip returns closed: open through the gap");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1,
		"tip returns closed: the gap is logged");

	joints[1].tip_tracked[1] = 1;
	joints[1].tip[1][2] = 0.010f; /* it comes back already pinched */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip returns closed: fires on the sample it returns");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		events[1].kind == HANDS_EVENT_PINCH && events[1].on == 1,
		"tip returns closed: the gap's end, then the press it was holding up");
}

/* a tip that drops out at 1.2 cm from the thumb, inside the closed band, and comes back at 6 cm:
the carry holds the pinch closed through the gap, and the live reading opens it on the sample it
returns, so a finger that really did move while invisible is corrected at once */
static void pinch_opens_when_the_tip_returns_apart(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.012f; /* 1.2 cm: under CLOSE, so the pinch is closed */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip returns apart: starts closed at 1.2 cm");

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip returns apart: the carry holds it closed");

	joints[1].tip_tracked[1] = 1;
	joints[1].tip[1][2] = 0.060f; /* it comes back 6 cm away: the finger did move */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tip returns apart: reads open on the sample it returns");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		events[1].kind == HANDS_EVENT_PINCH && events[1].on == 0,
		"tip returns apart: the gap's end, then the open the reading decided");
}

/* a hand ARKit has stopped reporting for longer than HANDS_HAND_GAP_SECONDS opens every one of
its pinches and zeros every one of its controls, on the sample the gap runs out: a hand lost
that long carries no fingertip forward, so nothing holds its pinches. The gap itself is what
the first untracked sample starts, and pinch_holds_through_a_hand_blink covers what it holds.
The hand that comes back closes at once on a live reading, with no apart-first confirmation */
static void hand_untracked_zeros(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count, finger, pinch_offs = 0;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	for (finger = 1; finger <= 4; finger++) /* every one of the gun hand's pinches closed */
		joints[1].tip[finger][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "untracked: starts closed");

	joints[1].tracked = 0; /* the hand's gap starts here, and holds everything while it runs */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "untracked gun hand: the first untracked sample holds");

	/* 0.20 s untracked, past the gap: now the hand is gone */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 16);
	check(controls_all_zero(&controls[1]), "untracked gun hand: every control zero once the gap has run out");
	for (finger = 0; finger < count; finger++)
		if (events[finger].kind == HANDS_EVENT_PINCH && events[finger].on == 0)
			pinch_offs++;
	check(count == NUMBER_OF_HANDS_PINCHES && pinch_offs == NUMBER_OF_HANDS_PINCHES,
		"untracked gun hand: every pinch opens on that same sample");

	joints[1].tracked = 1; /* back, with the fingers still together */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.4, controls, events, 16);
	check(controls[1].trigger == 1.0f, "untracked gun hand: a hand that comes back pinched closes at once");
}

/* a whole hand lost for less than HANDS_HAND_GAP_SECONDS is ARKit blinking, not a hand put
down: every pinch of that hand holds its state through the gap, the controls those pinches
drive hold with it, and the hand that comes back inside the gap resumes with no event at all,
exactly as a carried fingertip does (pinch_survives_a_tip_dropout). Without the gap a
whole-hand dropout of one or two samples releases the trigger and fires a second shot */
static void pinch_holds_through_a_hand_blink(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	double t;
	int count, events_seen = 0, dropped = 0;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* the gun hand's index pinch closed at t = 1.0 */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hand blink: starts closed");

	joints[1].tracked = 0; /* the whole hand is gone for 0.10 s, nine frames of a 90 Hz device */
	for (t = 1.05; t <= 1.15 + 1e-9; t += 0.05)
	{
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
		events_seen += count;
		if (controls[1].trigger != 1.0f)
			dropped++;
	}
	check(dropped == 0, "hand blink: the trigger never drops through a 0.10 s gap");
	check(events_seen == 0, "hand blink: no event at all while the gap runs");

	joints[1].tracked = 1; /* back inside the gap, the fingers still together */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hand blink: still closed as the hand returns");
	check(count == 0, "hand blink: the return itself says nothing");
}

/* the same hand gone for 0.20 s: HANDS_HAND_GAP_SECONDS has run out, so the pinch opens with
its event and every one of that hand's controls goes to zero on the sample it expires, which is
what a player lowering their hands is asking for */
static void hand_gap_expires(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hand gap: starts closed");

	joints[1].tracked = 0; /* the gap starts on this sample, so this one still holds */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "hand gap: the gap's first sample holds");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.25, controls, events, 16);
	check(controls[1].trigger == 0.0f, "hand gap: 0.20 s untracked opens the pinch");
	check(controls_all_zero(&controls[1]), "hand gap: every control of that hand is zero at the expiry");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0 &&
		events[0].finger == HANDS_PINCH_INDEX, "hand gap: one pinch-off event at the expiry");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 16);
	check(count == 0, "hand gap: an expired gap says nothing more while the hand stays away");
}

/* the aim turn stays gated on the hand's live tracked flag, not on the gap: the joint geometry
a blink holds is stale, so the forward the sampler hands in turns nothing and arms nothing while
the hand is away, even past the deadzone and even while that hand's pinches are still held */
static void aim_turn_silent_in_a_hand_gap(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* the gun hand's index closed, so the gap has something to hold */
	forward[2] = 0.0f;

	/* snap mode, armed inside the deadzone while the hand is tracked: a stale forward past the
	deadzone would otherwise pulse the stick the instant the hand blinks */
	forward[0] = cosf(10.0f * (float)M_PI / 180.0f); forward[1] = sinf(10.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f && controls[1].trigger == 1.0f, "aim turn in a gap: armed and closed");

	joints[1].tracked = 0;
	forward[0] = cosf(45.0f * (float)M_PI / 180.0f); forward[1] = sinf(45.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "aim turn in a gap: the pinch is still held");
	check(controls[1].stick_x == 0.0f, "aim turn in a gap: no snap pulse from a stale forward");
	check(count == 0, "aim turn in a gap: no event at all from a stale forward");

	/* smooth mode: a turn that was running goes to zero as the hand blinks, and reports itself
	off exactly as it does for a hand that stays untracked */
	memset(&state, 0, sizeof(state));
	joints[1] = make_hand(1);
	forward[0] = cosf(31.5f * (float)M_PI / 180.0f); forward[1] = sinf(31.5f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "aim turn in a gap: turning before the blink");

	joints[1].tracked = 0;
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.05, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim turn in a gap: the smooth turn contributes nothing");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN && events[0].on == 0,
		"aim turn in a gap: the turn reports itself off");
}

/* the off hand's index pinch moves the stick forward, middle back (index wins when both are
closed), ring clicks the stick; the gun hand's index fires, ring is the low face button and
middle is the high face button. Both hands' little fingers stay unbound */
static void off_hand_mapping(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[0].tip[1][2] = 0.010f; /* off hand (0): index closed */
	joints[0].tip[2][2] = 0.010f; /* off hand (0): middle closed too; index wins */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == 1.0f, "off hand: index wins over middle, stick_y +1");
	check(controls[0].menu == 0, "off hand: menu never set");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[0].tip[2][2] = 0.010f; /* only middle: backward */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == -1.0f, "off hand: middle alone, stick_y -1");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[0].tip[3][2] = 0.010f; /* ring: stick click */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_click == 1, "off hand: ring closes the stick click");
}

/* mirrored: aiming_hand = 0, so the gun is on hand 0 and the off hand is hand 1 */
static void mirror_aim(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[0].tip[1][2] = 0.010f; /* hand 0 is the gun hand now */
	hands_recognize(&state, joints, 0, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].trigger == 1.0f, "mirror: hand 0's index fires when it is the gun hand");
	check(controls[1].trigger == 0.0f, "mirror: hand 1 (the off hand) never fires from this pinch");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* walk forward with the off hand (now hand 1) */
	hands_recognize(&state, joints, 0, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].stick_y == 1.0f, "mirror: hand 1's index walks forward as the off hand");
	check(controls[1].menu == 0, "mirror: the off hand's menu is never set, mirrored too");
}

/* the off hand going untracked past its own gap zeros its own controls and leaves the gun
hand's standing, the mirror of hand_untracked_zeros. The gap is per hand, so the gun hand's
pinch is untouched by the off hand's loss at every point of it */
static void off_hand_untracked_zeros(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[0].tip[1][2] = 0.010f; /* the off hand walks forward */
	joints[1].tip[1][2] = 0.010f; /* and the gun hand fires */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == 1.0f && controls[1].trigger == 1.0f, "untracked off hand: both start pinched");

	joints[0].tracked = 0; /* the off hand's gap starts, so its stick still stands */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[0].stick_y == 1.0f, "untracked off hand: the gap holds its stick");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 16);
	check(controls_all_zero(&controls[0]), "untracked off hand: every control of its own zero past its gap");
	check(controls[1].trigger == 1.0f, "untracked off hand: the gun hand still fires");
}

/* the aim turn, smooth mode: deflection grows linearly over HANDS_AIM_TURN_RAMP_DEGREES past
the deadzone's edge, a left yaw turning left (negative stick x) */
static void aim_turn_smooth(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);

	forward[0] = cosf(10.0f * (float)M_PI / 180.0f); forward[1] = sinf(10.0f * (float)M_PI / 180.0f); forward[2] = 0.0f;
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim smooth: 10 degrees left is inside the deadzone");

	forward[0] = cosf(31.5f * (float)M_PI / 180.0f); forward[1] = sinf(31.5f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "aim smooth: 31.5 degrees left is -0.5");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN && events[0].on == 1 &&
		near(events[0].value, 31.5f, 0.1f) && near(events[0].threshold, 18.0f, 1e-3f),
		"aim smooth: one aim-turn-on event with the yaw against the deadzone");

	forward[0] = cosf(-45.0f * (float)M_PI / 180.0f); forward[1] = sinf(-45.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_x, 1.0f, 1e-3f), "aim smooth: 45 degrees right is +1");
	check(count == 0, "aim smooth: a turn already on says nothing as it crosses sides");

	forward[0] = cosf(5.0f * (float)M_PI / 180.0f); forward[1] = sinf(5.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.3, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim smooth: back inside the deadzone, no turn");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN && events[0].on == 0,
		"aim smooth: the turn's return to zero is logged");
}

/* the smooth aim turn over input.hand_turn_deadzone's whole accepted range (5 to 60 degrees,
host_spatial_hands.m's read_settings): the ramp runs HANDS_AIM_TURN_RAMP_DEGREES past whatever
the deadzone is, so a wide deadzone neither inverts the turn nor divides by zero */
static void aim_turn_wide_deadzone(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	forward[2] = 0.0f;

	forward[0] = cosf(20.0f * (float)M_PI / 180.0f); forward[1] = sinf(20.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 60.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim smooth: 20 degrees left is inside a 60 degree deadzone");

	/* 27 degrees past that deadzone: the ramp's full deflection, turning left for a left yaw */
	forward[0] = cosf(87.0f * (float)M_PI / 180.0f); forward[1] = sinf(87.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 60.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, -1.0f, 1e-3f), "aim smooth: 87 degrees left with a 60 degree deadzone is -1");

	/* a deadzone at 45 with the yaw right on its edge: the ramp is zero there, and clampf's own
	guard means no NaN reaches stick_x even if the two angles land a hair apart */
	memset(&state, 0, sizeof(state));
	forward[0] = cosf(45.0f * (float)M_PI / 180.0f); forward[1] = sinf(45.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 45.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_x, 0.0f, 1e-3f), "aim smooth: 45 degrees left with a 45 degree deadzone is zero");
}

/* the aim turn, snap mode: fires once past the deadzone, re-arms only once the yaw comes back
inside it */
static void aim_turn_snap(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);

	forward[0] = 1.0f; forward[1] = 0.0f; forward[2] = 0.0f; /* 0 degrees: arms it */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);

	forward[0] = cosf(30.0f * (float)M_PI / 180.0f); forward[1] = sinf(30.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[1].stick_x == -1.0f, "aim snap: 30 degrees left pulses -1");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN && events[0].on == 1, "aim snap: one aim-turn event");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 16); /* the pulse has released */
	check(controls[1].stick_x == 0.0f, "aim snap: the pulse releases");

	forward[0] = cosf(25.0f * (float)M_PI / 180.0f); forward[1] = sinf(25.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.4, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim snap: still out past the deadzone, but disarmed: no re-fire");

	forward[0] = cosf(10.0f * (float)M_PI / 180.0f); forward[1] = sinf(10.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.5, controls, events, 16); /* back inside: re-arms */

	forward[0] = cosf(30.0f * (float)M_PI / 180.0f); forward[1] = sinf(30.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.6, controls, events, 16);
	check(controls[1].stick_x == -1.0f, "aim snap: re-armed, pulses again");
	check(controls[0].menu == 0, "aim snap: the off hand's menu is never set");
}

/* an untracked gun hand turns nothing, whatever forward it is given: the yaw computation, the
aim-turn event and the armed/pulse state it would set are all gated on joints[g].tracked, the
hand's live flag rather than its gap. So the zeroed aim.forward the sampler passes for an
untracked hand (host_spatial_hands.m, which holds no last direction of its own) produces no
event either, not just a zeroed stick_x */
static void aim_turn_untracked_gun_hand(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float zero_forward[3] = { 0.0f, 0.0f, 0.0f };
	float forward40[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tracked = 0; /* the gun hand has never been tracked */

	count = hands_recognize(&state, joints, 1, zero_forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "untracked gun hand: no turn from a zero forward");
	check(count == 0, "untracked gun hand: no event from a zero forward");

	forward40[0] = cosf(40.0f * (float)M_PI / 180.0f); forward40[1] = sinf(40.0f * (float)M_PI / 180.0f);
	forward40[2] = 0.0f;
	count = hands_recognize(&state, joints, 1, forward40, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "untracked gun hand: no turn from a forward well past the deadzone");
	check(count == 0, "untracked gun hand: no event even well past the deadzone");
}

/* the gun hand's thumb-to-middle pinch fills the high face button: closed, it neither
suppresses nor feeds the aim turn, which fires past the deadzone exactly as it does with the
pinch open, and the pinch sets nothing on the gun hand's controls beyond that one button */
static void aim_turn_with_middle_pinch_closed(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[2][2] = 0.010f; /* the gun hand's middle closes */
	forward[0] = cosf(-45.0f * (float)M_PI / 180.0f); forward[1] = sinf(-45.0f * (float)M_PI / 180.0f); forward[2] = 0.0f;

	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_x, 1.0f, 1e-3f), "middle pinch closed: 45 degrees right still turns +1");
	check(controls[1].face_high == 1, "middle pinch closed: the pinch fills the high face button");
	check(controls[1].trigger == 0.0f && controls[1].face_low == 0,
		"middle pinch closed: no other control follows from it");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH && events[0].finger == HANDS_PINCH_MIDDLE &&
		events[0].on == 1 && events[1].kind == HANDS_EVENT_AIM_TURN && events[1].on == 1,
		"middle pinch closed: the pinch's own event, then the aim turn's, unrelated to each other");
}

/* the gun hand's middle pinch fills face_high and nothing else, and the off hand's middle
pinch still walks backward in the same sample: one hand's middle finger does not reach the
other hand's mapping */
static void middle_pinch_does_not_cross_hands(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[2][2] = 0.010f; /* gun hand (1): middle closed */
	joints[0].tip[2][2] = 0.010f; /* off hand (0): middle closed */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].face_high == 1, "gun hand: middle pinch fills the high face button");
	check(controls[1].trigger == 0.0f && controls[1].face_low == 0 && controls[1].grip == 0 &&
		controls[1].stick_click == 0 && controls[1].stick_x == 0.0f && controls[1].stick_y == 0.0f &&
		controls[1].menu == 0, "gun hand: middle pinch sets nothing else");
	check(controls[0].stick_y == -1.0f, "off hand: middle pinch still walks backward");
	check(controls[0].face_high == 0, "off hand: the gun hand's face button stays off the off hand");
}

/* both hands in the prayer sign: palms together and opposed per the pinned formula,
normal = normalize(cross(index_mc - wrist, little_mc - wrist)); swapping which metacarpal is
"index" between the two hands gives opposed normals without needing a measured sign */
static void prayer_pose(struct host_spatial_hand_joints joints[2])
{
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);

	joints[0].wrist[1] = 0.05f;
	joints[0].metacarpal[0][0] = 0.03f; joints[0].metacarpal[0][1] = 0.05f; joints[0].metacarpal[0][2] = 0.05f;
	joints[0].metacarpal[1][0] = -0.03f; joints[0].metacarpal[1][1] = 0.05f; joints[0].metacarpal[1][2] = 0.05f;

	joints[1].wrist[1] = -0.05f;
	joints[1].metacarpal[0][0] = -0.03f; joints[1].metacarpal[0][1] = -0.05f; joints[1].metacarpal[0][2] = 0.05f;
	joints[1].metacarpal[1][0] = 0.03f; joints[1].metacarpal[1][1] = -0.05f; joints[1].metacarpal[1][2] = 0.05f;
}

/* the prayer sign: both palms together, fingers up, a 0.6 s dwell that fires once and holds
Start for PULSE_SECONDS; a broken pose restarts the dwell, and a fresh hold inside the cooldown
never fires */
static void prayer(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	double t;
	int count;

	memset(&state, 0, sizeof(state));
	prayer_pose(joints);

	/* a 1.0 baseline keeps every timestamp strictly positive, away from the state's own
	zero-means-idle sentinels (prayer_since, pulse/cooldown clocks) */
	for (t = 1.0; t <= 1.55 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	check(controls[1].menu == 0, "prayer: not yet at 0.55 s held");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.6, controls, events, 16);
	check(controls[1].menu == 1, "prayer: fires at 0.6 s held");
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.69, controls, events, 16);
	check(controls[1].menu == 1, "prayer: Start still held at 0.69 s");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.71, controls, events, 16);
	check(controls[1].menu == 0, "prayer: Start released by 0.71 s");
	check(count == 1 && events[0].kind == HANDS_EVENT_START && events[0].on == 0,
		"prayer: the Start pulse's end is logged, so its width shows");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 3.0, controls, events, 16);
	check(controls[1].menu == 0, "prayer: no re-fire from a continued hold");
	check(controls[0].menu == 0, "prayer: the off hand's menu is never set");

	/* break the pose, then resume: the dwell restarts and does not fire early */
	memset(&state, 0, sizeof(state));
	for (t = 1.0; t <= 1.3; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	joints[0].wrist[1] = 0.5f; /* the pose breaks: too far apart */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 16);
	joints[0].wrist[1] = 0.05f; /* resumes */
	for (t = 1.35; t <= 1.6 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	check(controls[1].menu == 0, "prayer: a break restarts the dwell, no fire by the old schedule's 0.6 s");

	/* a fresh hold starting inside the cooldown never fires */
	memset(&state, 0, sizeof(state));
	for (t = 1.0; t <= 1.6 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	check(controls[1].menu == 1, "prayer cooldown setup: the first hold fires");
	joints[0].wrist[1] = 0.5f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.8, controls, events, 16); /* release */
	joints[0].wrist[1] = 0.05f;
	for (t = 1.8; t <= 1.8 + 0.2 + 0.6 + 1e-9; t += 0.05) /* a new hold 0.2 s after the fire */
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	check(controls[1].menu == 0, "prayer: a hold that begins inside the cooldown never fires");
}

/* the Start pulse the prayer sign just earned outlives the hands dropping out of sight
inside its own window: bringing both hands together and then letting them fall is the natural
thing to do, and the pause would otherwise be eaten. Two things hold it now, since each hand's
gap already outlasts HANDS_PULSE_SECONDS; the exception in hands_recognize's own zeroing is
what would still hold a pulse wider than that gap */
static void start_pulse_outlives_the_hands(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	double t;

	memset(&state, 0, sizeof(state));
	prayer_pose(joints);
	joints[1].tip[1][2] = 0.010f; /* the gun hand's index closed too, to zero with the rest */
	for (t = 1.0; t <= 1.6 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	check(controls[1].menu == 1, "start pulse: the sign fires at 0.6 s held");

	joints[0].tracked = joints[1].tracked = 0; /* both hands gone the very next sample */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.62, controls, events, 16);
	check(controls[1].menu == 1, "start pulse: still held with both hands untracked inside the window");
	check(controls[1].trigger == 1.0f, "start pulse: the gun hand's index is held by its own gap");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.75, controls, events, 16);
	check(controls[1].menu == 0, "start pulse: released once the window ends, hands or no hands");

	/* and the hands' own gap runs out a little later, which is where the pinches go */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.8, controls, events, 16);
	check(controls_all_zero(&controls[1]), "start pulse: every control of the gun hand zero past its gap");
}

/* a pinch reading a carried thumb or fingertip reports the gap it is in, on as the gap starts
and off with its length as the gap ends: the carry is what holds the pinch's state through it,
so the log is the only thing that says the reading was not a live one */
static void pinch_miss_logged(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* the gun hand's index pinch closed at t = 1.0 */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);

	joints[1].tip_tracked[0] = 0; /* the thumb drops: every one of that hand's four pinches carries it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch miss: the closed pinch holds through the gap");
	check(count == 4 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX, "pinch miss: one miss-on event per carried pinch");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.35, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch miss: a gap of any length never opens the pinch");
	check(count == 0, "pinch miss: a gap already logged says nothing more while it runs");

	joints[1].tip_tracked[0] = 1; /* the thumb returns: each gap ends with its own length */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.4, controls, events, 16);
	check(count == 4 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		near(events[0].value, 0.35f, 1e-3f), "pinch miss: each gap's length is logged as it ends");
}

/* a sample that changes more states than capacity holds (both hands losing all four pinches
at once: up to 8 pinch-off events) returns a count clamped to capacity, so a caller's
events[0..count) is always in bounds */
static void event_capacity_clamped(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[2];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count, finger;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	for (finger = 1; finger <= 4; finger++) /* every pinch on both hands, closed */
	{
		joints[0].tip[finger][2] = 0.010f;
		joints[1].tip[finger][2] = 0.010f;
	}
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 2); /* capacity matches the
		buffer on every call: this setup sample emits more than 2 events too, into the same events[2] */

	joints[0].tracked = 0; /* both hands untracked: the sample their gaps start on holds everything */
	joints[1].tracked = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 2);

	/* past both gaps: every one of the 8 closed pinches opens at once */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 2);
	check(count == 2, "event capacity: the return clamps to capacity, not the true 8");
	check(events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0, "event capacity: events[0] is a valid pinch-off");
	check(events[1].kind == HANDS_EVENT_PINCH && events[1].on == 0, "event capacity: events[1] is a valid pinch-off");
}

/* the defect the part check exists for: a pinch occludes the very fingertip whose distance
closed it, so the tip goes missing on the sample after the press and the carried position
says "still on the thumb" for as long as the gap runs. While the fingers really are together
that is the right answer and nothing may come out of it. The moment they part, the pair of
joints the thumb is not sitting on grows away from what it measured while the tips were last
seen, and the pinch releases there and then rather than when the camera finds the tip again */
static void pinch_parts_while_the_tip_is_hidden(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	double t;
	int count, events_seen = 0, opened = 0;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* the index pinch closes on a live reading at t = 1.0 */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 1 && events[0].path == HANDS_PATH_MEASURED,
		"parts: the press is measured from the tips");

	joints[1].tip_tracked[1] = 0; /* and the pinch hides the tip immediately, as on device */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].path == HANDS_PATH_CARRIED, "parts: the gap opens as a carried one");

	for (t = 1.022; t <= 4.022 + 1e-9; t += 0.011) /* 3.0 s with the fingers still together */
	{
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
		events_seen += count;
		if (controls[1].trigger != 1.0f)
			opened++;
	}
	check(opened == 0, "parts: closed through 3.0 s of the tip being hidden");
	check(events_seen == 0, "parts: a gap with the fingers together says nothing at all");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.050f); /* the fingers part, the tip still hidden */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 4.033, controls, events, 16);
	check(controls[1].trigger == 0.0f, "parts: the trigger releases on the sample the fingers part");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0 &&
		events[0].path == HANDS_PATH_PARTED, "parts: one pinch-off event, named as the pair's");
	check(near(events[0].value, 0.020f, 1e-4f) && events[0].threshold == HANDS_PART_MARGIN_METERS,
		"parts: the event carries the growth against the margin");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 4.044, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 0,
		"parts: the carry cannot close it again while the pair says apart");
}

/* a tap: the press, the tip hidden by it, and the fingers parting again inside 200 ms. The
release lands on the sample the parting is first visible, which is what makes a tap one shot
instead of a held trigger (on the plasma pistol, a controlled shot instead of a charged one) */
static void pinch_tap_releases_within_one_sample(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tap: pressed at t = 1.0");

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tap: held while the tip is hidden");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.045f); /* 1.5 cm of parting, 22 ms in */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tap: released 22 ms after the press, inside one sample of parting");
	check(count == 1 && events[0].on == 0 && events[0].path == HANDS_PATH_PARTED,
		"tap: the release is the pair's, not a tip that came back");
}

/* hold to fire: automatic weapons are fired by holding the pinch, and the tip is hidden for
every sample of it. The release rule has no clock and nothing that accumulates in it, only the
pair's separation now against what it was when the tips were last measured, and rigid motion
of the hand cannot change that separation. So 5 s of samples with the hand turning and
travelling never releases, where a deadline on the gap's length would have */
static void pinch_hold_to_fire_never_releases(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	const float along[3] = { 0.002f, 0.0f, 0.0f };
	double t;
	int count, events_seen = 0, opened = 0, samples = 0;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hold to fire: pressed, and the tip hidden by it");

	for (t = 1.022; t <= 6.022 + 1e-9; t += 0.011) /* 5.0 s at 90 Hz, the hand moving throughout */
	{
		move_hand(&joints[1], 1.0f, along);
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
		events_seen += count;
		samples++;
		if (controls[1].trigger != 1.0f)
			opened++;
	}
	check(samples > 400, "hold to fire: the 5.0 s really is 400 samples and more");
	check(opened == 0, "hold to fire: the trigger is never released");
	check(events_seen == 0, "hold to fire: and nothing is logged against it");

	joints[1].tip_tracked[1] = 1; /* the tip is seen again, still on the thumb */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 6.033, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS &&
		events[0].on == 0, "hold to fire: still closed as the tip returns, with only the gap's length logged");
}

/* the thumb's intermediate tip is the more distal half of the pair and the one that goes
first, since the pinching finger wraps over the pad behind it. With it untracked the check
falls to the thumb's intermediate base, a bone further back and on the outside of the hand,
and still releases */
static void pinch_part_uses_the_thumb_ladder(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "thumb ladder: starts closed with both levels measured");

	joints[1].tip_tracked[1] = 0;
	joints[1].intermediate_tip_tracked[0] = 0; /* the thumb's own distal joint goes with the tip */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "thumb ladder: held with only the thumb's base left");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.050f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 0.0f, "thumb ladder: the second level releases it");
	check(count == 1 && events[0].path == HANDS_PATH_PARTED && near(events[0].value, 0.020f, 1e-4f),
		"thumb ladder: the growth is the same 2.0 cm, measured from that level's own reference");
}

/* the pair untracked as well as the tip: no reading on either side, so the check has nothing
to say and the carry's answer stands, which is today's behavior of holding the pinch closed.
A missing pair must never be read as a parting -- that would release the trigger every time
ARKit lost a few joints at once */
static void pinch_part_falls_back_without_the_pair(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);

	joints[1].tip_tracked[1] = 0;
	joints[1].intermediate_base_tracked[1] = 0; /* the finger's half of the pair, which both levels share */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);
	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.050f); /* parted, but nothing can see it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0,
		"no pair: an unreadable pair holds the pinch, exactly as the carry alone did");

	joints[1].intermediate_base_tracked[1] = 1; /* the joint comes back, and the parting with it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 1 && events[0].path == HANDS_PATH_PARTED,
		"no pair: the release waits for the pair rather than being guessed without it");
}

/* a pinch with nothing to carry either: the metacarpals go, so there is no palm frame to read
a hidden tip back out of (host_spatial_hands.c's tips_resolve) and the pinch is blind rather
than guessing. The pair is still tracked, and it releases, because the check is ahead of the
early return a pinch with no reading takes */
static void pinch_parts_with_nothing_to_carry(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);

	joints[1].tip_tracked[1] = 0;
	joints[1].metacarpal_tracked[0] = joints[1].metacarpal_tracked[1] = 0;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "blind pinch: held with no palm frame to carry the tip in");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX && events[0].path == HANDS_PATH_BLIND,
		"blind pinch: the index's gap is logged as a blind one, not as a carried one");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.050f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 1 && events[0].path == HANDS_PATH_PARTED,
		"blind pinch: the pair releases it even with no reading of the tips at all");
}

/* the part check is a band, like the pinch's own: it releases above the margin and lets a
carried position close the pinch again only below half of it. Without the lower edge a pair
sitting near the margin would release and re-close sample after sample, which is a trigger
firing repeatedly from one held pinch. Below the lower edge the fingers really are back
together, and a re-press while the tip is still hidden is the player pinching again */
static void pinch_part_does_not_chatter(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.011, controls, events, 16);

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.050f); /* 2.0 cm: past the margin, released */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 1, "chatter: released past the margin");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.039f); /* 0.9 cm: inside the band, still apart */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 0, "chatter: a pair back inside the band stays released");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.034f); /* 0.4 cm: together again, a real re-press */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.044, controls, events, 16);
	check(controls[1].trigger == 1.0f, "chatter: the fingers back together close it again");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 1 &&
		events[0].path == HANDS_PATH_CARRIED, "chatter: and that press is logged as the carry's, not the tips'");
}

int main(void)
{
	pinch_hysteresis();
	pinch_survives_a_tip_dropout();
	pinch_carry_follows_the_palm();
	pinch_closes_when_the_tip_returns();
	pinch_opens_when_the_tip_returns_apart();
	hand_untracked_zeros();
	pinch_holds_through_a_hand_blink();
	hand_gap_expires();
	aim_turn_silent_in_a_hand_gap();
	off_hand_untracked_zeros();
	off_hand_mapping();
	mirror_aim();
	aim_turn_smooth();
	aim_turn_wide_deadzone();
	aim_turn_snap();
	aim_turn_untracked_gun_hand();
	aim_turn_with_middle_pinch_closed();
	middle_pinch_does_not_cross_hands();
	prayer();
	start_pulse_outlives_the_hands();
	pinch_miss_logged();
	pinch_parts_while_the_tip_is_hidden();
	pinch_tap_releases_within_one_sample();
	pinch_hold_to_fire_never_releases();
	pinch_part_uses_the_thumb_ladder();
	pinch_part_falls_back_without_the_pair();
	pinch_parts_with_nothing_to_carry();
	pinch_part_does_not_chatter();
	event_capacity_clamped();
	if (failures)
	{
		printf("hands probe: %d FAILED\n", failures);
		return 1;
	}
	printf("hands probe: OK\n");
	return 0;
}
