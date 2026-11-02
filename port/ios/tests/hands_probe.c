/* The gesture recognizer (port/ios/host/host_spatial_hands.c, included):
pinch hysteresis and the fingertip carried in the palm's frame through a
dropout, the release a pinch gets from the joints behind it once the fingers
part while that fingertip is still hidden (and that holding the pinch never
earns one), the growth that release is measured by and the line it is reported
on, the length of the hold every release reports behind itself, the gun and
off hand's button and stick mapping, the aim turn (smooth and snap) and the
arm's elevation beside it on the same stick, with the sign that makes a
raised arm raise the gun, the prayer sign's dwell and Start pulse, the
whole-hand gap a blink is held through, and that a hand untracked past that
gap opens every one of its pinches and zeros every one of its own
controls. */
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
		/* the chain walking back from each tip, three joints of it: the thumb's toward the
		wrist, each finger's down the finger. Every level of the part check's ladder
		(host_spatial_hands.c's part_pair_of) lies along this one axis, so a level's
		separation is a difference of two of these numbers and set_part_joint below moves a
		level by moving one of its ends. The first level, a finger's intermediate tip against
		the thumb's, starts at 5.0 cm, and the tests read its growth against
		HANDS_PART_MARGIN_METERS */
		j.intermediate_tip_tracked[i] = 1;
		j.intermediate_base_tracked[i] = 1;
		j.knuckle_tracked[i] = 1;
		j.intermediate_tip[i][2] = (i == 0) ? -0.020f : 0.030f;
		j.intermediate_base[i][2] = (i == 0) ? -0.040f : 0.010f;
		j.knuckle[i][2] = (i == 0) ? -0.060f : -0.010f;
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
	float *points[24];
	int i, count = 0;

	points[count++] = j->wrist;
	points[count++] = j->forearm_arm;
	for (i = 0; i < 5; i++)
	{
		points[count++] = j->tip[i];
		/* the part check's own joints move with the rest or the hold-to-fire case below
		proves nothing: a pair left behind while every other joint turns keeps its
		separation for the wrong reason. Every level of the ladder is covered, not only the
		one a given sample happens to read */
		points[count++] = j->intermediate_tip[i];
		points[count++] = j->intermediate_base[i];
		points[count++] = j->knuckle[i];
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

/* one of the pinching finger's joints put a given distance from the thumb's intermediate tip,
which is the thumb end every level of the ladder prefers (host_spatial_hands.c's
part_pair_of): the joints the check reads, and none of the fingertips. That is the whole case
the check is for, since on device the tips are what the pinching thumb hides. Which finger
joint is moved is the caller's, because which of them moves is the whole question the device
answered: moving the intermediate tip alone is a real parting that the middle knuckle does
not see */
static void set_part_joint(const struct host_spatial_hand_joints *j, float joint[3], float meters)
{
	joint[0] = j->intermediate_tip[0][0];
	joint[1] = j->intermediate_tip[0][1];
	joint[2] = j->intermediate_tip[0][2] + meters;
}

/* the whole hand parting: every one of the pinching finger's three joints moved out together,
which is what a finger really extending out of a pinch does. make_hand leaves the first level
at 5.0 cm, so 7.0 cm is 2.0 cm of parting at every level of the ladder at once */
static void set_part_pair(struct host_spatial_hand_joints *j, int finger, float meters)
{
	set_part_joint(j, j->intermediate_tip[finger + 1], meters);
	set_part_joint(j, j->intermediate_base[finger + 1], meters - 0.020f);
	set_part_joint(j, j->knuckle[finger + 1], meters - 0.040f);
}

/* an aim forward the recognizer's two angles read straight back off, both in degrees: the
yaw about the room's up axis and the elevation out of its horizontal plane
(host_spatial_hands.c's hands_recognize takes the yaw as atan2(forward[1], forward[0]) and
the elevation as the rise against the length of that horizontal part). The game's axes, x
forward, y left, z up, as host_spatial_pose.h's host_spatial_room_to_game leaves them */
static void set_aim(float forward[3], float yaw_degrees, float elevation_degrees)
{
	float yaw = yaw_degrees * (float)M_PI / 180.0f, elevation = elevation_degrees * (float)M_PI / 180.0f;

	forward[0] = cosf(elevation) * cosf(yaw);
	forward[1] = cosf(elevation) * sinf(yaw);
	forward[2] = sinf(elevation);
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
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch: 1.0 cm closes the trigger");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].hand == 1 &&
		events[0].finger == HANDS_PINCH_INDEX && events[0].on == 1, "pinch: one pinch-on event for the index");

	/* 1.8 cm: inside the hysteresis band. The band is scaled off this finger's own ruler now
	(host_spatial_hands.c's ruler_meters), so the value has to sit inside it both while the
	ruler is still HANDS_RULER_DEFAULT_METERS, as it is here on three samples, and at the 2.0 cm
	segment make_hand's own joints would converge it to over a longer case: 1.49 to 2.24 cm and
	1.33 to 2.0 cm respectively, and 1.8 is inside both. 2.4 cm was inside the old fixed band
	and is outside both of these, which is the measurement this change acted on */
	joints[1].tip[1][2] = 0.018f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch: 1.8 cm still closed (hysteresis)");
	check(count == 0, "pinch: no event inside the hysteresis band");

	joints[1].tip[1][2] = 0.040f; /* 4.0 cm: opens */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch: 4.0 cm opens the trigger");
	/* the release the two tips decided, and the pair's growth reported beside it: every one of
	these carries the margin's measurement, with nothing hidden and no gap behind it, so the
	growth is 0 and the line says so (host_spatial_hands.h's HANDS_EVENT_PART_GROWTH) */
	check(count == 3 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0 &&
		events[2].kind == HANDS_EVENT_PART_GROWTH && events[2].on == 0 &&
		near(events[2].value, 0.0f, 1e-4f), "pinch: one pinch-off event, with the growth beside it");
	/* and the hold's own length between the two, which is the measurement the pinch rules have
	never had: closed at t = 1.0, released at t = 1.2 (host_spatial_hands.h's
	HANDS_EVENT_PINCH_HELD) */
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && events[1].hand == 1 &&
		events[1].finger == HANDS_PINCH_INDEX && events[1].on == 0 && near(events[1].value, 0.2f, 1e-3f) &&
		events[1].threshold == 0.0f && events[1].path == HANDS_PATH_MEASURED,
		"pinch: and the 0.2 s it was held, on the path that released it");
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip dropout: starts closed");

	joints[1].tip_tracked[1] = 0; /* the index tip drops out at t = 1.05 */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip dropout: still closed as the tip drops out");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX, "tip dropout: the gap is logged as a miss, not as a pinch");

	for (t = 1.10; t <= 4.05 + 1e-9; t += 0.05) /* 3.0 s of it */
	{
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
		for (i = 0; i < count; i++)
			if (events[i].kind == HANDS_EVENT_PINCH)
				pinch_events++;
		if (controls[1].trigger != 1.0f)
			opened++;
	}
	check(opened == 0, "tip dropout: closed through 3.0 s of dropout");
	check(pinch_events == 0, "tip dropout: no pinch event during the gap");

	joints[1].tip_tracked[1] = 1; /* the tip returns, still pinched */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 4.1, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: starts closed");

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: closed as the tip drops out");

	move_hand(&joints[1], 40.0f, across);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.5, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: closed with the hand turned 40 degrees and moved 0.37 m");
	check(count == 0, "carried tip: the move itself says nothing");

	move_hand(&joints[1], -70.0f, back);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 2.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "carried tip: closed again with the hand turned back the other way");
	check(count == 0, "carried tip: the second move says nothing either");

	joints[1].tip_tracked[1] = 1; /* the tip is seen again where the moved palm says it is */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 2.5, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tip returns closed: starts open, the tips 5 cm from the thumb");

	joints[1].tip_tracked[1] = 0;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tip returns closed: open through the gap");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1,
		"tip returns closed: the gap is logged");

	joints[1].tip_tracked[1] = 1;
	joints[1].tip[1][2] = 0.010f; /* it comes back already pinched */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip returns apart: starts closed at 1.2 cm");

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tip returns apart: the carry holds it closed");

	joints[1].tip_tracked[1] = 1;
	joints[1].tip[1][2] = 0.060f; /* it comes back 6 cm away: the finger did move */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tip returns apart: reads open on the sample it returns");
	check(count == 4 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		events[1].kind == HANDS_EVENT_PINCH && events[1].on == 0 &&
		events[2].kind == HANDS_EVENT_PINCH_HELD && events[3].kind == HANDS_EVENT_PART_GROWTH,
		"tip returns apart: the gap's end, the open the reading decided, the hold's length and the growth");
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
	int count, finger, pinch_offs = 0, helds = 0;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	for (finger = 1; finger <= 4; finger++) /* every one of the gun hand's pinches closed */
		joints[1].tip[finger][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "untracked: starts closed");

	joints[1].tracked = 0; /* the hand's gap starts here, and holds everything while it runs */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "untracked gun hand: the first untracked sample holds");

	/* 0.20 s untracked, past the gap: now the hand is gone */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 16);
	check(controls_all_zero(&controls[1]), "untracked gun hand: every control zero once the gap has run out");
	for (finger = 0; finger < count; finger++)
	{
		if (events[finger].kind == HANDS_EVENT_PINCH && events[finger].on == 0)
			pinch_offs++;
		/* the whole-hand open is a release like any other, so each of the four reports the hold
		it ended: closed at t = 1.0 and lost at t = 1.3, on the hand-lost path */
		if (events[finger].kind == HANDS_EVENT_PINCH_HELD && near(events[finger].value, 0.3f, 1e-3f) &&
			events[finger].path == HANDS_PATH_HAND_LOST)
			helds++;
	}
	check(count == 2 * NUMBER_OF_HANDS_PINCHES && pinch_offs == NUMBER_OF_HANDS_PINCHES,
		"untracked gun hand: every pinch opens on that same sample");
	check(helds == NUMBER_OF_HANDS_PINCHES,
		"untracked gun hand: and each one reports the 0.3 s it was held when the hand went");

	joints[1].tracked = 1; /* back, with the fingers still together */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.4, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hand blink: starts closed");

	joints[1].tracked = 0; /* the whole hand is gone for 0.10 s, nine frames of a 90 Hz device */
	for (t = 1.05; t <= 1.15 + 1e-9; t += 0.05)
	{
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
		events_seen += count;
		if (controls[1].trigger != 1.0f)
			dropped++;
	}
	check(dropped == 0, "hand blink: the trigger never drops through a 0.10 s gap");
	check(events_seen == 0, "hand blink: no event at all while the gap runs");

	joints[1].tracked = 1; /* back inside the gap, the fingers still together */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hand gap: starts closed");

	joints[1].tracked = 0; /* the gap starts on this sample, so this one still holds */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "hand gap: the gap's first sample holds");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.25, controls, events, 16);
	check(controls[1].trigger == 0.0f, "hand gap: 0.20 s untracked opens the pinch");
	check(controls_all_zero(&controls[1]), "hand gap: every control of that hand is zero at the expiry");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0 &&
		events[0].finger == HANDS_PINCH_INDEX, "hand gap: one pinch-off event at the expiry");
	/* with the hold it ended: closed at t = 1.0, the hand gone by t = 1.25, so the trigger was
	down for the whole 0.25 s including the gap that opened it */
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 0.25f, 1e-3f) &&
		events[1].path == HANDS_PATH_HAND_LOST, "hand gap: and the 0.25 s of trigger the gap ended");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f && controls[1].trigger == 1.0f, "aim turn in a gap: armed and closed");

	joints[1].tracked = 0;
	forward[0] = cosf(45.0f * (float)M_PI / 180.0f); forward[1] = sinf(45.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "aim turn in a gap: the pinch is still held");
	check(controls[1].stick_x == 0.0f, "aim turn in a gap: no snap pulse from a stale forward");
	check(count == 0, "aim turn in a gap: no event at all from a stale forward");

	/* smooth mode: a turn that was running goes to zero as the hand blinks, and reports itself
	off exactly as it does for a hand that stays untracked */
	memset(&state, 0, sizeof(state));
	joints[1] = make_hand(1);
	forward[0] = cosf(31.5f * (float)M_PI / 180.0f); forward[1] = sinf(31.5f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "aim turn in a gap: turning before the blink");

	joints[1].tracked = 0;
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.05, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == 1.0f, "off hand: index wins over middle, stick_y +1");
	check(controls[0].menu == 0, "off hand: menu never set");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[0].tip[2][2] = 0.010f; /* only middle: backward */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == -1.0f, "off hand: middle alone, stick_y -1");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[0].tip[3][2] = 0.010f; /* ring: stick click */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
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
	hands_recognize(&state, joints, 0, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[0].trigger == 1.0f, "mirror: hand 0's index fires when it is the gun hand");
	check(controls[1].trigger == 0.0f, "mirror: hand 1 (the off hand) never fires from this pinch");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* walk forward with the off hand (now hand 1) */
	hands_recognize(&state, joints, 0, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == 1.0f && controls[1].trigger == 1.0f, "untracked off hand: both start pinched");

	joints[0].tracked = 0; /* the off hand's gap starts, so its stick still stands */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[0].stick_y == 1.0f, "untracked off hand: the gap holds its stick");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim smooth: 10 degrees left is inside the deadzone");

	forward[0] = cosf(31.5f * (float)M_PI / 180.0f); forward[1] = sinf(31.5f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "aim smooth: 31.5 degrees left is -0.5");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN && events[0].on == 1 &&
		near(events[0].value, 31.5f, 0.1f) && near(events[0].threshold, 18.0f, 1e-3f),
		"aim smooth: one aim-turn-on event with the yaw against the deadzone");

	forward[0] = cosf(-45.0f * (float)M_PI / 180.0f); forward[1] = sinf(-45.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_x, 1.0f, 1e-3f), "aim smooth: 45 degrees right is +1");
	check(count == 0, "aim smooth: a turn already on says nothing as it crosses sides");

	forward[0] = cosf(5.0f * (float)M_PI / 180.0f); forward[1] = sinf(5.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.3, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 0, 60.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim smooth: 20 degrees left is inside a 60 degree deadzone");

	/* 27 degrees past that deadzone: the ramp's full deflection, turning left for a left yaw */
	forward[0] = cosf(87.0f * (float)M_PI / 180.0f); forward[1] = sinf(87.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 60.0f, 0.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, -1.0f, 1e-3f), "aim smooth: 87 degrees left with a 60 degree deadzone is -1");

	/* a deadzone at 45 with the yaw right on its edge: the ramp is zero there, and clampf's own
	guard means no NaN reaches stick_x even if the two angles land a hair apart */
	memset(&state, 0, sizeof(state));
	forward[0] = cosf(45.0f * (float)M_PI / 180.0f); forward[1] = sinf(45.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 45.0f, 0.0f, 1.0, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);

	forward[0] = cosf(30.0f * (float)M_PI / 180.0f); forward[1] = sinf(30.0f * (float)M_PI / 180.0f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].stick_x == -1.0f, "aim snap: 30 degrees left pulses -1");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN && events[0].on == 1, "aim snap: one aim-turn event");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 16); /* the pulse has released */
	check(controls[1].stick_x == 0.0f, "aim snap: the pulse releases");

	forward[0] = cosf(25.0f * (float)M_PI / 180.0f); forward[1] = sinf(25.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.4, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim snap: still out past the deadzone, but disarmed: no re-fire");

	forward[0] = cosf(10.0f * (float)M_PI / 180.0f); forward[1] = sinf(10.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.5, controls, events, 16); /* back inside: re-arms */

	forward[0] = cosf(30.0f * (float)M_PI / 180.0f); forward[1] = sinf(30.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.6, controls, events, 16);
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

	count = hands_recognize(&state, joints, 1, zero_forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "untracked gun hand: no turn from a zero forward");
	check(count == 0, "untracked gun hand: no event from a zero forward");

	forward40[0] = cosf(40.0f * (float)M_PI / 180.0f); forward40[1] = sinf(40.0f * (float)M_PI / 180.0f);
	forward40[2] = 0.0f;
	count = hands_recognize(&state, joints, 1, forward40, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "untracked gun hand: no turn from a forward well past the deadzone");
	check(count == 0, "untracked gun hand: no event even well past the deadzone");

	/* and the same for the elevation, which shares the gate: an arm raised 40 degrees on a hand
	ARKit is not reporting elevates nothing and says nothing */
	set_aim(forward40, 0.0f, 40.0f);
	count = hands_recognize(&state, joints, 1, forward40, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(controls[1].stick_y == 0.0f, "untracked gun hand: no elevation from a forward well past the deadzone");
	check(count == 0, "untracked gun hand: no elevation event either");
}

/* the arm's elevation on the aiming stick's vertical axis, shaped as the smooth turn is: zero
inside input.hand_turn_deadzone, a linear ramp over HANDS_AIM_TURN_RAMP_DEGREES past it, and
clamped at the ends. The sign is the one thing here that could only be read out of the game:
controls[g].stick_y reaches input_abstraction.c as the right stick's vertical axis and
becomes input_state->pitch, which player_control.c reads as looking up while it is positive,
so a raised arm has to deflect positive and a lowered one negative */
static void aim_elevation_smooth(void)
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

	set_aim(forward, 0.0f, 0.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].stick_y == 0.0f, "aim elevation: a level arm elevates nothing");
	check(count == 0, "aim elevation: and says nothing about it");

	set_aim(forward, 0.0f, 10.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].stick_y == 0.0f, "aim elevation: 10 degrees up is inside the deadzone");

	set_aim(forward, 0.0f, 31.5f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_y, 0.5f, 1e-3f), "aim elevation: 31.5 degrees up is +0.5, raising the gun");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 1 &&
		events[0].hand == 1 && near(events[0].value, 31.5f, 0.1f) && near(events[0].threshold, 18.0f, 1e-3f),
		"aim elevation: one elevation-on event with the elevation against the deadzone");

	set_aim(forward, 0.0f, 45.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.3, controls, events, 16);
	check(near(controls[1].stick_y, 1.0f, 1e-3f), "aim elevation: 45 degrees up is the ramp's full +1");
	check(count == 0, "aim elevation: an elevation already on says nothing as it ramps");

	set_aim(forward, 0.0f, 80.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.4, controls, events, 16);
	check(near(controls[1].stick_y, 1.0f, 1e-3f), "aim elevation: an arm straight up clamps at +1");

	/* the mirror: an arm lowered the same amount deflects the same amount the other way, which
	is what points a turret's gun down */
	set_aim(forward, 0.0f, -31.5f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.5, controls, events, 16);
	check(near(controls[1].stick_y, -0.5f, 1e-3f), "aim elevation: 31.5 degrees down is -0.5, lowering the gun");
	check(count == 0, "aim elevation: crossing level without passing through the deadzone says nothing");

	set_aim(forward, 0.0f, -45.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.6, controls, events, 16);
	check(near(controls[1].stick_y, -1.0f, 1e-3f), "aim elevation: 45 degrees down is the ramp's full -1");

	set_aim(forward, 0.0f, -5.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.7, controls, events, 16);
	check(controls[1].stick_y == 0.0f, "aim elevation: back inside the deadzone, no elevation");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 0,
		"aim elevation: the return to level is logged");
}

/* the elevation ignores input.hand_turn_snap, where the turn does not: a snap is a discrete
step of yaw and there is no such thing as a snapped elevation, so a player who picked snap
turning would otherwise have no way at all to aim a turret up or down */
static void aim_elevation_ignores_snap(void)
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

	set_aim(forward, 0.0f, 45.0f);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_y, 1.0f, 1e-3f), "aim elevation: snap turning still elevates");
	check(controls[1].stick_x == 0.0f, "aim elevation: and the yaw inside the deadzone snaps nothing");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 1,
		"aim elevation: the elevation reports itself in snap mode too");
}

/* input.aim_pitch_offset corrects the elevation the same way controller_aim_game.c corrects
the barrel ray: an arm held in the player's own natural aiming pose reads as level once their
offset is subtracted, rather than as that offset's own worth of elevation. Without this trim
a Covenant turret's gun climbed to its stop on every device session, since a level aim read
as the full offset (27 degrees for the player who found it) and that was already past the
deadzone (f7bfc0312 added the stick, this fixes what it read) */
static void aim_elevation_pitch_offset(void)
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

	/* the natural pose itself: 27 degrees of raw elevation, trimmed level by a 27 degree
	offset, is the case on device that flew the turret to its stop before this fix */
	set_aim(forward, 0.0f, 27.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 27.0f, 1.0, controls, events, 16);
	check(controls[1].stick_y == 0.0f, "pitch offset: a natural pose trimmed level elevates nothing");
	check(count == 0, "pitch offset: and says nothing about it");

	/* 20 degrees up from that trimmed zero: 47 degrees raw, 20 once trimmed, 2 past the 18
	degree deadzone */
	set_aim(forward, 0.0f, 47.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 27.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_y, 2.0f / 27.0f, 1e-3f),
		"pitch offset: 20 degrees up from the trimmed zero raises the gun");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 1 &&
		near(events[0].value, 20.0f, 0.1f) && near(events[0].threshold, 18.0f, 1e-3f),
		"pitch offset: the elevation event carries the trimmed angle, not the raw one");

	/* 20 degrees down from the trimmed zero instead: 7 degrees raw, -20 once trimmed. The jump
	passes straight through the trimmed zero without a sample inside the deadzone, exactly as
	aim_elevation_smooth's own crossing does, so this reports nothing new */
	set_aim(forward, 0.0f, 7.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 27.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_y, -2.0f / 27.0f, 1e-3f),
		"pitch offset: 20 degrees down from the trimmed zero lowers the gun");
	check(count == 0, "pitch offset: crossing the trimmed zero without passing through the deadzone says nothing");
}

/* the regression this fixes, pinned to the angle that found it: the same natural pose with no
offset applied reads the full 27 degrees of raw elevation and deflects the stick, which is what
climbed the turret before input.aim_pitch_offset was read on this path at all */
static void aim_elevation_without_pitch_offset(void)
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

	set_aim(forward, 0.0f, 27.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_y, 9.0f / 27.0f, 1e-3f),
		"pitch offset: without it the same natural pose still deflects the stick, which is the regression");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 1,
		"pitch offset: and reports the elevation turning on");
}

/* the trim only ever reaches the elevation (host_spatial_hands.c's hands_recognize keeps the
yaw on the raw aim_forward): the same arm turns the stick by the same amount and logs the same
AIM_TURN value whatever input.aim_pitch_offset is set to */
static void aim_turn_ignores_pitch_offset(void)
{
	struct hands_state untrimmed_state, trimmed_state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls untrimmed_controls[2], trimmed_controls[2];
	struct hands_event untrimmed_events[16], trimmed_events[16];
	float forward[3];
	int untrimmed_count, trimmed_count;

	joints[0] = make_hand(0);
	joints[1] = make_hand(1);

	/* the elevation stays inside the 18 degree deadzone both trimmed and untrimmed (12, and
	12 - 27 = -15), so neither run's elevation axis has anything to say and the only event
	either logs is the turn being compared */
	set_aim(forward, 31.5f, 12.0f);

	memset(&untrimmed_state, 0, sizeof(untrimmed_state));
	untrimmed_count = hands_recognize(&untrimmed_state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0,
		untrimmed_controls, untrimmed_events, 16);

	memset(&trimmed_state, 0, sizeof(trimmed_state));
	trimmed_count = hands_recognize(&trimmed_state, joints, 1, forward, 0, 18.0f, 27.0f, 1.0,
		trimmed_controls, trimmed_events, 16);

	check(near(untrimmed_controls[1].stick_x, trimmed_controls[1].stick_x, 1e-6f),
		"pitch offset: the turn deflects identically whatever the trim is");
	check(untrimmed_count == 1 && trimmed_count == 1 &&
		untrimmed_events[0].kind == HANDS_EVENT_AIM_TURN && trimmed_events[0].kind == HANDS_EVENT_AIM_TURN &&
		near(untrimmed_events[0].value, trimmed_events[0].value, 1e-6f),
		"pitch offset: and logs the same yaw either way");
}

/* the two axes of the one stick are read off the same aim forward and neither touches the
other: a yaw past the deadzone with the arm level turns and does not elevate, and raising the
arm from there elevates without disturbing the turn or logging anything about it */
static void aim_axes_do_not_interfere(void)
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

	set_aim(forward, 31.5f, 0.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "two axes: 31.5 degrees left turns -0.5");
	check(controls[1].stick_y == 0.0f, "two axes: and a level arm elevates nothing while it turns");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_TURN, "two axes: only the turn is logged");

	set_aim(forward, 31.5f, 31.5f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "two axes: raising the arm leaves the turn where it was");
	check(near(controls[1].stick_y, 0.5f, 1e-3f), "two axes: and elevates +0.5 beside it");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 1,
		"two axes: only the elevation is logged, the turn having not changed");

	/* and back the other way: dropping the arm to level ends the elevation alone */
	set_aim(forward, 31.5f, 0.0f);
	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "two axes: the turn still stands");
	check(controls[1].stick_y == 0.0f, "two axes: the elevation alone has gone");
	check(count == 1 && events[0].kind == HANDS_EVENT_AIM_ELEVATION && events[0].on == 0,
		"two axes: and only the elevation's end is logged");
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

	count = hands_recognize(&state, joints, 1, forward, 0, 18.0f, 0.0f, 1.0, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
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
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
	check(controls[1].menu == 0, "prayer: not yet at 0.55 s held");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.6, controls, events, 16);
	check(controls[1].menu == 1, "prayer: fires at 0.6 s held");
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.69, controls, events, 16);
	check(controls[1].menu == 1, "prayer: Start still held at 0.69 s");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.71, controls, events, 16);
	check(controls[1].menu == 0, "prayer: Start released by 0.71 s");
	check(count == 1 && events[0].kind == HANDS_EVENT_START && events[0].on == 0,
		"prayer: the Start pulse's end is logged, so its width shows");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 3.0, controls, events, 16);
	check(controls[1].menu == 0, "prayer: no re-fire from a continued hold");
	check(controls[0].menu == 0, "prayer: the off hand's menu is never set");

	/* break the pose, then resume: the dwell restarts and does not fire early */
	memset(&state, 0, sizeof(state));
	for (t = 1.0; t <= 1.3; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
	joints[0].wrist[1] = 0.5f; /* the pose breaks: too far apart */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 16);
	joints[0].wrist[1] = 0.05f; /* resumes */
	for (t = 1.35; t <= 1.6 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
	check(controls[1].menu == 0, "prayer: a break restarts the dwell, no fire by the old schedule's 0.6 s");

	/* a fresh hold starting inside the cooldown never fires */
	memset(&state, 0, sizeof(state));
	for (t = 1.0; t <= 1.6 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
	check(controls[1].menu == 1, "prayer cooldown setup: the first hold fires");
	joints[0].wrist[1] = 0.5f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.8, controls, events, 16); /* release */
	joints[0].wrist[1] = 0.05f;
	for (t = 1.8; t <= 1.8 + 0.2 + 0.6 + 1e-9; t += 0.05) /* a new hold 0.2 s after the fire */
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
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
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
	check(controls[1].menu == 1, "start pulse: the sign fires at 0.6 s held");

	joints[0].tracked = joints[1].tracked = 0; /* both hands gone the very next sample */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.62, controls, events, 16);
	check(controls[1].menu == 1, "start pulse: still held with both hands untracked inside the window");
	check(controls[1].trigger == 1.0f, "start pulse: the gun hand's index is held by its own gap");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.75, controls, events, 16);
	check(controls[1].menu == 0, "start pulse: released once the window ends, hands or no hands");

	/* and the hands' own gap runs out a little later, which is where the pinches go */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.8, controls, events, 16);
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);

	joints[1].tip_tracked[0] = 0; /* the thumb drops: every one of that hand's four pinches carries it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch miss: the closed pinch holds through the gap");
	check(count == 4 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX, "pinch miss: one miss-on event per carried pinch");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.35, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch miss: a gap of any length never opens the pinch");
	check(count == 0, "pinch miss: a gap already logged says nothing more while it runs");

	joints[1].tip_tracked[0] = 1; /* the thumb returns: each gap ends with its own length */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.4, controls, events, 16);
	check(count == 4 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		near(events[0].value, 0.35f, 1e-3f), "pinch miss: each gap's length is logged as it ends");
}

/* the length of a hold, which is the thing three rounds of pinch work never measured
(host_spatial_hands.h's HANDS_EVENT_PINCH_HELD): a tap and a deliberate hold in the same run
each report their own duration, measured from their own press, and a whole-hand blink in the
middle of the hold neither releases it nor restarts its clock. Nothing here reads the
duration back into a decision: the pinch rules are exactly what they were, and these lines
exist so the next change to them can be a number rather than a fourth guess */
static void pinch_hold_duration_logged(void)
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

	/* a tap: pressed at t = 10.0, let go 80 ms later */
	joints[1].tip[1][2] = 0.010f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 10.0, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 1, "hold length: the tap's press, with no hold line behind it");

	joints[1].tip[1][2] = 0.040f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 10.08, controls, events, 16);
	check(controls[1].trigger == 0.0f, "hold length: the tap releases");
	check(count == 3 && events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 0.08f, 1e-3f) &&
		events[1].hand == 1 && events[1].finger == HANDS_PINCH_INDEX && events[1].on == 0 &&
		events[1].path == HANDS_PATH_MEASURED, "hold length: and reports the 80 ms it was held");

	/* a deliberate hold, with a blink in it: pressed at t = 11.0, the hand gone for one sample
	well inside HANDS_HAND_GAP_SECONDS, released at t = 12.5 */
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 11.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hold length: pressed again at t = 11.0");

	joints[1].tracked = 0;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 11.05, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "hold length: a blink holds the trigger silently");
	joints[1].tracked = 1;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 11.1, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "hold length: and the hand's return is not a fresh press");

	joints[1].tip[1][2] = 0.040f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 12.5, controls, events, 16);
	check(controls[1].trigger == 0.0f, "hold length: the hold releases");
	check(count == 3 && events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 1.5f, 1e-3f),
		"hold length: reporting the whole 1.5 s from the press, blink included");
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 2); /* capacity matches the
		buffer on every call: this setup sample emits more than 2 events too, into the same events[2] */

	joints[0].tracked = 0; /* both hands untracked: the sample their gaps start on holds everything */
	joints[1].tracked = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 2);

	/* past both gaps: every one of the 8 closed pinches opens at once */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 2);
	check(count == 2, "event capacity: the return clamps to capacity, not the true 8");
	check(events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0, "event capacity: events[0] is a valid pinch-off");
	/* events[1] is that pinch's own hold line, since every release writes one right behind
	itself (host_spatial_hands.c's held_ended): the pair survives the clamp together, which is
	the point of sizing the caller's buffer for both */
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && events[1].on == 0 && events[1].hand == events[0].hand &&
		events[1].finger == events[0].finger, "event capacity: events[1] is that release's own hold line");
}

/* the defect the part check exists for: a pinch occludes the very fingertip whose distance
closed it, so the tip goes missing on the sample after the press and the carried position
says "still on the thumb" for as long as the gap runs. While the fingers really are together
that is the right answer and nothing may come out of it. The moment they part, the pair of
joints the thumb is not sitting on grows away from what it measured while the tips were last
seen, and the pinch releases on the next sample rather than when the camera finds the tip
again (HANDS_PART_CONFIRM_SAMPLES: the parting has to be seen twice running) */
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
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 1 && events[0].path == HANDS_PATH_MEASURED,
		"parts: the press is measured from the tips");

	joints[1].tip_tracked[1] = 0; /* and the pinch hides the tip immediately, as on device */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].path == HANDS_PATH_CARRIED, "parts: the gap opens as a carried one");

	for (t = 1.022; t <= 4.022 + 1e-9; t += 0.011) /* 3.0 s with the fingers still together */
	{
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
		events_seen += count;
		if (controls[1].trigger != 1.0f)
			opened++;
	}
	check(opened == 0, "parts: closed through 3.0 s of the tip being hidden");
	check(events_seen == 0, "parts: a gap with the fingers together says nothing at all");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.070f); /* the fingers part, the tip still hidden */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 4.033, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0,
		"parts: the first sample past the margin arms the release rather than taking it");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 4.044, controls, events, 16);
	check(controls[1].trigger == 0.0f, "parts: the trigger releases on the sample that confirms the parting");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0 &&
		events[0].path == HANDS_PATH_PARTED, "parts: one pinch-off event, named as the pair's");
	check(near(events[0].value, 0.020f, 1e-4f) && events[0].threshold == HANDS_PART_MARGIN_METERS,
		"parts: the event carries the growth against the margin");
	/* and the length of that hold beside it, which is what makes a session's taps countable:
	closed at t = 1.0, held deliberately for three seconds, released at t = 4.044 */
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 3.044f, 1e-3f) &&
		events[1].path == HANDS_PATH_PARTED, "parts: with the 3.044 s of trigger the pair ended");

	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 4.055, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 0,
		"parts: the carry cannot close it again while the pair says apart");
}

/* the device case the first design missed, and the reason for the joint it now watches. A
finger opening out of a pinch moves almost entirely at the fingertip: the middle knuckle (the
intermediate base) barely translates at all, so a parting seen only there never grew by the
margin and the first design's one release in a whole session was all it could ever manage.
Here only the finger's intermediate tip moves, by 2.0 cm, with the middle knuckle and the
knuckle held exactly where the press left them. The pinch must still release */
static void pinch_parts_at_the_intermediate_tip_alone(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	float held_base[3], held_knuckle[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "distal joint: pressed at t = 1.0");
	memcpy(held_base, joints[1].intermediate_base[HANDS_PINCH_INDEX + 1], sizeof(held_base));
	memcpy(held_knuckle, joints[1].knuckle[HANDS_PINCH_INDEX + 1], sizeof(held_knuckle));

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);

	/* the one joint that travels, and nothing else: 2.0 cm clears the margin several times
	over, so a failure here is the joint choice and not the margin */
	set_part_joint(&joints[1], joints[1].intermediate_tip[HANDS_PINCH_INDEX + 1], 0.070f);
	check(!memcmp(held_base, joints[1].intermediate_base[HANDS_PINCH_INDEX + 1], sizeof(held_base)) &&
		!memcmp(held_knuckle, joints[1].knuckle[HANDS_PINCH_INDEX + 1], sizeof(held_knuckle)),
		"distal joint: the middle knuckle and the knuckle really did not move");
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f, "distal joint: the joint behind the fingertip releases the pinch");
	check(count == 2 && events[0].path == HANDS_PATH_PARTED && near(events[0].value, 0.020f, 1e-4f),
		"distal joint: the growth is that joint's own 2.0 cm");
}

/* the margin's two edges, with the tip hidden throughout: a growth below
HANDS_PART_MARGIN_METERS holds the pinch, and a growth above it releases it on the next
sample. The numbers are deliberately close to the margin, since that is where a provisional
constant is wrong if it is wrong */
static void pinch_part_margin_edges(void)
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.054f); /* 4 mm, under the margin */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "margin: 4 mm of growth holds the pinch");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "margin: and holds it for as long as it stays there");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.056f); /* 6 mm, over it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.044, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "margin: 6 mm arms the release on the sample it appears");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.055, controls, events, 16);
	check(controls[1].trigger == 0.0f, "margin: 6 mm releases on the very next sample, 11 ms later");
	check(count == 2 && events[0].path == HANDS_PATH_PARTED && near(events[0].value, 0.006f, 1e-4f),
		"margin: the release carries the 6 mm that earned it");
	/* a tap of exactly this shape is the complaint's subject, and it now has a number: closed
	at t = 1.0, released at t = 1.055 */
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 0.055f, 1e-3f),
		"margin: and the 55 ms that tap was held");
}

/* the measurement that settles the margin. Every pinch release the two fingertips themselves
decided also reports what the pair behind them had grown by at that instant, so a session of
ordinary play gives the distribution of growths at a release the player meant -- which is the
thing 12 mm was guessed against and 5 mm still is. The line is emitted whatever the growth
was, including the growths well under the margin, because those are exactly the releases the
part check is failing to catch */
static void pinch_part_growth_logged(void)
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.054f); /* 4 mm: not enough to release */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "growth: held on 4 mm, as the margin says");

	/* the tip comes back apart, which is the release the device log was full of: the pinch
	waited for the camera rather than for the fingers */
	joints[1].tip_tracked[1] = 1;
	joints[1].tip[1][2] = 0.040f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 4,
		"growth: the gap's end, the release, the hold's length and the growth behind it");
	check(events[1].kind == HANDS_EVENT_PINCH && events[1].on == 0 && events[1].path == HANDS_PATH_MEASURED,
		"growth: the release itself is the tips'");
	/* the release this file exists to catch, now with the figure the complaint is about: a tap
	closed at t = 1.0 that the camera, not the fingers, let go at t = 1.033 */
	check(events[2].kind == HANDS_EVENT_PINCH_HELD && events[2].finger == HANDS_PINCH_INDEX &&
		events[2].hand == 1 && near(events[2].value, 0.033f, 1e-3f) && events[2].threshold == 0.0f &&
		events[2].path == HANDS_PATH_MEASURED, "growth: the hold's own length, on the path that ended it");
	check(events[3].kind == HANDS_EVENT_PART_GROWTH && events[3].finger == HANDS_PINCH_INDEX &&
		events[3].hand == 1, "growth: and the growth line names the same pinch");
	check(near(events[3].value, 0.004f, 1e-4f) && events[3].threshold == HANDS_PART_MARGIN_METERS,
		"growth: carrying the 4 mm the pair had grown, against the margin that did not catch it");
	check(events[3].on == 1 && events[3].path == HANDS_PATH_MEASURED,
		"growth: marked as spanning a gap, with the pair readable throughout");

	/* a release with no gap before it: the reference is one sample old, so the growth is not a
	measurement of anything, and the line says so by coming out off rather than on */
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.044, controls, events, 16);
	joints[1].tip[1][2] = 0.040f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.055, controls, events, 16);
	check(count == 3 && events[2].kind == HANDS_EVENT_PART_GROWTH && events[2].on == 0 &&
		near(events[2].value, 0.0f, 1e-4f), "growth: a release with no gap before it reports a growth of nothing");
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 0.011f, 1e-3f),
		"growth: and that press was held for the one sample between it and the release");
}

/* a tap: the press, the tip hidden by it, and the fingers parting again inside 200 ms. The
release lands on the sample that confirms the parting, one sample (11 ms at 90 Hz) after it
first shows, which is what makes a tap one shot instead of a held trigger (on the plasma
pistol, a controlled shot instead of a charged one). The whole tap is 33 ms of trigger, far
inside the second or so the plasma pistol takes to charge */
static void pinch_tap_releases_on_the_confirming_sample(void)
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tap: pressed at t = 1.0");

	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "tap: held while the tip is hidden");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.065f); /* 1.5 cm of parting, 22 ms in */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "tap: the parting's first sample is the confirmation's");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f, "tap: released 33 ms after the press, one sample after the parting showed");
	check(count == 2 && events[0].on == 0 && events[0].path == HANDS_PATH_PARTED,
		"tap: the release is the pair's, not a tip that came back");
	/* and the number that makes a device session readable: 33 ms of trigger for a tap, against
	the hold a player complains about (host_spatial_hands.h's HANDS_EVENT_PINCH_HELD) */
	check(events[1].kind == HANDS_EVENT_PINCH_HELD && near(events[1].value, 0.033f, 1e-3f) &&
		events[1].path == HANDS_PATH_PARTED, "tap: logged as 33 ms held, not as a hold");
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "hold to fire: pressed, and the tip hidden by it");

	for (t = 1.022; t <= 6.022 + 1e-9; t += 0.011) /* 5.0 s at 90 Hz, the hand moving throughout */
	{
		move_hand(&joints[1], 1.0f, along);
		count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, t, controls, events, 16);
		events_seen += count;
		samples++;
		if (controls[1].trigger != 1.0f)
			opened++;
	}
	check(samples > 400, "hold to fire: the 5.0 s really is 400 samples and more");
	check(opened == 0, "hold to fire: the trigger is never released");
	check(events_seen == 0, "hold to fire: and nothing is logged against it");

	joints[1].tip_tracked[1] = 1; /* the tip is seen again, still on the thumb */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 6.033, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS &&
		events[0].on == 0, "hold to fire: still closed as the tip returns, with only the gap's length logged");
}

/* the thumb's intermediate tip is the thumb end every level prefers, and the one that goes
first, since the pinching finger wraps over the pad behind it. With it untracked the ladder
steps sideways to the thumb's intermediate base, a bone further back and on the outside of
the hand, keeping the finger's intermediate tip as the end that actually travels, and still
releases */
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "thumb ladder: starts closed with every level measured");

	joints[1].tip_tracked[1] = 0;
	joints[1].intermediate_tip_tracked[0] = 0; /* the thumb's own distal joint goes with the tip */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "thumb ladder: held with only the thumb's base left");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.070f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f, "thumb ladder: the level on the thumb's base releases it");
	check(count == 2 && events[0].path == HANDS_PATH_PARTED && near(events[0].value, 0.020f, 1e-4f),
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);

	joints[1].tip_tracked[1] = 0;
	/* the finger's whole half of the ladder, every joint of it: with none of its three left
	there is no level the check can read, whichever thumb joint is paired with them */
	joints[1].intermediate_tip_tracked[1] = 0;
	joints[1].intermediate_base_tracked[1] = 0;
	joints[1].knuckle_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);
	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.070f); /* parted, but nothing can see it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0,
		"no pair: an unreadable pair holds the pinch, exactly as the carry alone did");
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0,
		"no pair: and holds it for as long as the pair stays unreadable");

	joints[1].intermediate_tip_tracked[1] = 1; /* the joints come back, and the parting with them */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.044, controls, events, 16);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.055, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 2 && events[0].path == HANDS_PATH_PARTED,
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);

	joints[1].tip_tracked[1] = 0;
	joints[1].metacarpal_tracked[0] = joints[1].metacarpal_tracked[1] = 0;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);
	check(controls[1].trigger == 1.0f, "blind pinch: held with no palm frame to carry the tip in");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX && events[0].path == HANDS_PATH_BLIND,
		"blind pinch: the index's gap is logged as a blind one, not as a carried one");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.070f);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 2 && events[0].path == HANDS_PATH_PARTED,
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
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	joints[1].tip_tracked[1] = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.011, controls, events, 16);

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.070f); /* 2.0 cm: past the margin, released */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.022, controls, events, 16);
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.033, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 2, "chatter: released past the margin");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.054f); /* 4 mm: inside the band, still apart */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.044, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 0, "chatter: a pair back inside the band stays released");

	set_part_pair(&joints[1], HANDS_PINCH_INDEX, 0.052f); /* 2 mm: together again, a real re-press */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.055, controls, events, 16);
	check(controls[1].trigger == 1.0f, "chatter: the fingers back together close it again");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 1 &&
		events[0].path == HANDS_PATH_CARRIED, "chatter: and that press is logged as the carry's, not the tips'");
}

/* one finger's ruler filled from a chosen segment length: its intermediate tip put that far
behind its own tip, then enough tracked samples run that host_spatial_hands.c's ruler_meters
takes the median of real readings instead of returning HANDS_RULER_DEFAULT_METERS. Twenty
samples where the window holds fifteen, so the window is saturated with this one value and the
two or three measuring samples a case runs afterwards cannot move the median off it, however
the fixture's own tip moves happen to read as segments. The tip stays where make_hand leaves
it, 5 cm from the thumb, so nothing closes while the window fills */
static void fill_ruler(struct hands_state *state, struct host_spatial_hand_joints joints[2], int hand,
	int finger, float segment_meters)
{
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	int i;

	joints[hand].intermediate_tip[finger + 1][2] = joints[hand].tip[finger + 1][2] - segment_meters;
	for (i = 0; i < 20; i++)
		hands_recognize(state, joints, 1, forward, 1, 18.0f, 0.0f, 0.1 + 0.001 * i, controls, events, 16);
}

/* the band comes off the player's own finger rather than off two absolute distances. A ruler of
2.24 cm is the index segment a device session measured, and the band it gives is 1.493 cm and
2.24 cm: the clear air between that session's pinching distribution (95th percentile 2.06 cm)
and its resting one (minimum 1.83 cm), which neither of the old constants was in
(host_spatial_hands.h's HANDS_PINCH_CLOSE_RULER_FRACTION carries the percentiles).
Every event reports the threshold it was decided against, so debug.hand_log on device shows
the scaled number and not a constant */
static void pinch_band_scales_with_the_ruler(void)
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
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.0224f);

	joints[1].tip[1][2] = 0.0149f; /* a hair under two thirds of the ruler: closes */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "ruler: 1.49 cm closes against a 2.24 cm ruler");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 1 &&
		near(events[0].threshold, 0.0224f * 2.0f / 3.0f, 1e-5f),
		"ruler: and the event carries the scaled close threshold, not a constant");

	joints[1].tip[1][2] = 0.0223f; /* a hair under one whole ruler: inside the band, no change */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "ruler: 2.23 cm is still inside the band");
	check(count == 0, "ruler: and says nothing");

	joints[1].tip[1][2] = 0.0225f; /* a hair over one whole ruler: opens */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "ruler: 2.25 cm opens against a 2.24 cm ruler");
	check(count == 3 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0 &&
		near(events[0].threshold, 0.0224f, 1e-5f),
		"ruler: and the release carries the scaled open threshold");

	/* and the other edge of the close, on its own state: a hair over two thirds of the ruler is
	above the resting minimum this band was placed under, and must not fire */
	memset(&state, 0, sizeof(state));
	joints[1] = make_hand(1);
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.0224f);
	joints[1].tip[1][2] = 0.0151f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 0.0f, "ruler: 1.51 cm does not close against a 2.24 cm ruler");
	check(count == 0, "ruler: and nothing is logged about it");
}

/* a larger hand gets a proportionally larger band off its own fingers, which is the whole point
of a fraction: a 3.0 cm segment closes at 2.0 cm and opens at 3.0 cm, the same two thirds and
one whole the 2.24 cm segment above gives 1.49 and 2.24 cm */
static void pinch_band_scales_with_a_larger_hand(void)
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
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.0300f);

	joints[1].tip[1][2] = 0.0199f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "larger ruler: 1.99 cm closes against a 3.0 cm ruler");
	check(count == 1 && near(events[0].threshold, 0.0200f, 1e-5f),
		"larger ruler: the close threshold scaled with the finger");

	/* 2.5 cm closes nothing on the smaller hand above and sits inside this one's band */
	joints[1].tip[1][2] = 0.0250f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f && count == 0, "larger ruler: 2.5 cm is inside the larger band");

	joints[1].tip[1][2] = 0.0301f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "larger ruler: 3.01 cm opens against a 3.0 cm ruler");
	check(count == 3 && near(events[0].threshold, 0.0300f, 1e-5f),
		"larger ruler: the open threshold scaled with it too");
}

/* each finger is measured on its own, because each finger is a different length: the middle,
ring and little fingers' distal segments are shorter than the index's, and on device their
resting thumb distances run near 8 cm where the index's runs near 2.5. So one thumb distance can
be a pinch on one finger and not on another, and that is correct rather than a bug: 1.6 cm is
inside a 3.0 cm finger's close and outside a 2.24 cm finger's */
static void pinch_ruler_is_per_finger(void)
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
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.0224f);
	fill_ruler(&state, joints, 1, HANDS_PINCH_MIDDLE, 0.0300f);

	joints[1].tip[1][2] = 0.0160f;
	joints[1].tip[2][2] = 0.0160f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 0.0f, "per finger: 1.6 cm misses the index's close at 1.49 cm");
	check(controls[1].face_high == 1, "per finger: and makes the middle finger's at 2.0 cm");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].finger == HANDS_PINCH_MIDDLE &&
		events[0].on == 1 && near(events[0].threshold, 0.0200f, 1e-5f),
		"per finger: one event, the middle finger's, against the middle finger's own threshold");
}

/* a segment that cannot be a finger leaves the thresholds at the measured default rather than
at nothing: ruler_push drops a reading outside HANDS_RULER_MINIMUM_METERS to
HANDS_RULER_MAXIMUM_METERS, so a window that only ever sees glitches stays empty and
ruler_meters answers HANDS_RULER_DEFAULT_METERS. Without that a segment read as 0 would scale
both thresholds to 0, which closes no pinch ever and opens every one of them */
static void pinch_ruler_falls_back_without_a_plausible_segment(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	/* the segment read as exactly 0, the case a zero threshold would come out of */
	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.0f);
	joints[1].tip[1][2] = 0.0149f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "ruler fallback: a zero segment still closes at the default band");
	check(count == 1 && near(events[0].threshold, HANDS_RULER_DEFAULT_METERS * 2.0f / 3.0f, 1e-5f),
		"ruler fallback: against the default ruler's own close, not 0");

	/* and a segment far too long to be one, the other way a glitch reads */
	memset(&state, 0, sizeof(state));
	joints[1] = make_hand(1);
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.200f);
	joints[1].tip[1][2] = 0.0151f;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 0.0f && count == 0,
		"ruler fallback: a 20 cm segment leaves the default band, which 1.51 cm does not close");
}

/* the ruler is the one thing on a pinch that outlives losing the hand. Everything else there is
a reading of a pose and is cleared when ARKit stops reporting the hand (host_spatial_hands.c's
pinch_update, its !hand_tracked branch), but a finger's length is a fact about the player and
the hand that comes back has the fingers that left. A hand lowered and raised again mid-firefight
would otherwise spend five samples on the default band every time */
static void pinch_ruler_outlives_a_lost_hand(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	fill_ruler(&state, joints, 1, HANDS_PINCH_INDEX, 0.0300f);

	/* the whole hand gone for longer than HANDS_HAND_GAP_SECONDS, which is a real loss */
	joints[1].tracked = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.0, controls, events, 16);
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.2, controls, events, 16);

	/* back, and pinching at 1.99 cm: inside the 3.0 cm ruler's close and well outside the
	default ruler's, so this closing is the ruler having survived */
	joints[1].tracked = 1;
	joints[1].tip[1][2] = 0.0199f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 0.0f, 1.3, controls, events, 16);
	check(controls[1].trigger == 1.0f, "ruler across a loss: the hand comes back with the finger it left with");
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
	aim_elevation_smooth();
	aim_elevation_ignores_snap();
	aim_elevation_pitch_offset();
	aim_elevation_without_pitch_offset();
	aim_turn_ignores_pitch_offset();
	aim_axes_do_not_interfere();
	aim_turn_with_middle_pinch_closed();
	middle_pinch_does_not_cross_hands();
	prayer();
	start_pulse_outlives_the_hands();
	pinch_miss_logged();
	pinch_parts_while_the_tip_is_hidden();
	pinch_parts_at_the_intermediate_tip_alone();
	pinch_part_margin_edges();
	pinch_part_growth_logged();
	pinch_tap_releases_on_the_confirming_sample();
	pinch_hold_to_fire_never_releases();
	pinch_part_uses_the_thumb_ladder();
	pinch_part_falls_back_without_the_pair();
	pinch_parts_with_nothing_to_carry();
	pinch_part_does_not_chatter();
	pinch_hold_duration_logged();
	pinch_band_scales_with_the_ruler();
	pinch_band_scales_with_a_larger_hand();
	pinch_ruler_is_per_finger();
	pinch_ruler_falls_back_without_a_plausible_segment();
	pinch_ruler_outlives_a_lost_hand();
	event_capacity_clamped();
	if (failures)
	{
		printf("hands probe: %d FAILED\n", failures);
		return 1;
	}
	printf("hands probe: OK\n");
	return 0;
}
