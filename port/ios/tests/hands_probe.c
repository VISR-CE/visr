/* The gesture recognizer (port/ios/host/host_spatial_hands.c, included):
pinch hysteresis and the tip-loss hold, the gun and off hand's button and
stick mapping, the aim turn (smooth and snap), the prayer sign's dwell and
Start pulse, and that a hand with tracked == 0 zeros every one of its own
controls the same sample. */
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

	joints[1].tip[1][2] = 0.020f; /* 2.0 cm: inside the hysteresis band, no change */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch: 2.0 cm still closed (hysteresis)");
	check(count == 0, "pinch: no event inside the hysteresis band");

	joints[1].tip[1][2] = 0.030f; /* 3.0 cm: opens */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.2, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch: 3.0 cm opens the trigger");
	check(count == 1 && events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0, "pinch: one pinch-off event");
	check(controls[0].menu == 0, "pinch: the off hand's menu is never set");
}

/* a tip lost mid-pinch holds the pinch for HOLD_SECONDS, then opens; once opened by a loss, the
pinch does not close again until a tracked sample reads the distance above OPEN first */
static void pinch_loss_hold(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* close the index pinch at t = 1.0 */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch loss: starts closed");

	joints[1].tip_tracked[0] = 0; /* the thumb tip goes untracked at t = 1.05: the loss timer starts here */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch loss: still closed right as the thumb drops out");

	/* 0.3 s later, comfortably past HOLD_SECONDS (0.2 s): opens */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.35, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch loss: open after HOLD_SECONDS untracked");

	joints[1].tip_tracked[0] = 1; /* tracked again, at 1.0 cm: must not re-close yet */
	joints[1].tip[1][2] = 0.010f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.4, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch loss: still open at 1.0 cm right after the loss (no confirmation yet)");

	joints[1].tip[1][2] = 0.030f; /* a sample above OPEN confirms it */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.5, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch loss: still open above OPEN");

	joints[1].tip[1][2] = 0.010f; /* now it can close again */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.6, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch loss: closes again once confirmed");
}

/* a hand with tracked == 0 opens every one of its pinches and zeros every one of its controls,
the same sample, with no hold */
static void hand_untracked_zeros(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand(0);
	joints[1] = make_hand(1);
	joints[1].tip[1][2] = 0.010f; /* the gun hand's index pinch closed */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].trigger == 1.0f, "untracked: starts closed");

	joints[1].tracked = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls_all_zero(&controls[1]), "untracked gun hand: every control zero the same sample");
}

/* the off hand's index pinch moves the stick forward, middle back (index wins when both are
closed), little clicks the stick; the gun hand's index fires and ring is the low face button */
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
	joints[0].tip[4][2] = 0.010f; /* little: stick click */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_click == 1, "off hand: little closes the stick click");
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

/* the off hand going untracked zeros its own controls the same sample and leaves the gun
hand's standing, the mirror of hand_untracked_zeros */
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

	joints[0].tracked = 0;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 16);
	check(controls_all_zero(&controls[0]), "untracked off hand: every control of its own zero the same sample");
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
same signal the final memset reads to zero controls[g]. So the zeroed aim.forward the sampler
passes for an untracked hand (host_spatial_hands.m, which holds no last direction of its own)
produces no event either, not just a zeroed stick_x */
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

/* the gun hand's thumb-to-middle pinch is unbound: closed, it neither suppresses nor feeds the
aim turn, which fires past the deadzone exactly as it does with the pinch open, and the pinch
itself sets no control of its own (unlike the off hand's pinches, the gun hand's middle has no
mapping at all) */
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
	check(controls[1].trigger == 0.0f && controls[1].face_low == 0,
		"middle pinch closed: the pinch itself sets no control");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH && events[0].finger == HANDS_PINCH_MIDDLE &&
		events[0].on == 1 && events[1].kind == HANDS_EVENT_AIM_TURN && events[1].on == 1,
		"middle pinch closed: the pinch's own event, then the aim turn's, unrelated to each other");
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
thing to do, and the untracked memset would otherwise eat the pause */
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
	check(controls[1].trigger == 0.0f, "start pulse: the rest of the gun hand's controls are still zeroed");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.75, controls, events, 16);
	check(controls[1].menu == 0, "start pulse: released once the window ends, hands or no hands");
}

/* a pinch blocked by an untracked thumb or fingertip reports the gap it is in: the hold is
all that stands between it and an open, and the reading itself is gone, so the log says so
rather than falling silent */
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

	joints[1].tip_tracked[0] = 0; /* the thumb drops: every one of that hand's four pinches is blocked */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].trigger == 1.0f, "pinch miss: the closed pinch holds through the gap");
	check(count == 4 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 1 &&
		events[0].finger == HANDS_PINCH_INDEX && near(events[0].threshold, (float)HANDS_PINCH_HOLD_SECONDS, 1e-3f),
		"pinch miss: one miss-on event per blocked pinch, against the hold");

	/* 0.3 s in, past HOLD_SECONDS: the gap ends and the pinch opens with it */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.35, controls, events, 16);
	check(controls[1].trigger == 0.0f, "pinch miss: the hold opens the pinch");
	check(count == 2 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0 &&
		near(events[0].value, 0.3f, 1e-3f) && events[1].kind == HANDS_EVENT_PINCH && events[1].on == 0,
		"pinch miss: the gap's length is logged, then the open it caused");

	joints[1].tip_tracked[0] = 1; /* the thumb returns: the three pinches still in a gap close theirs */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.4, controls, events, 16);
	check(count == 3 && events[0].kind == HANDS_EVENT_PINCH_MISS && events[0].on == 0,
		"pinch miss: a gap that ends before the hold is logged too");
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

	joints[0].tracked = 0; /* both hands untracked: every one of the 8 closed pinches opens */
	joints[1].tracked = 0;
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.1, controls, events, 2);
	check(count == 2, "event capacity: the return clamps to capacity, not the true 8");
	check(events[0].kind == HANDS_EVENT_PINCH && events[0].on == 0, "event capacity: events[0] is a valid pinch-off");
	check(events[1].kind == HANDS_EVENT_PINCH && events[1].on == 0, "event capacity: events[1] is a valid pinch-off");
}

int main(void)
{
	pinch_hysteresis();
	pinch_loss_hold();
	hand_untracked_zeros();
	off_hand_untracked_zeros();
	off_hand_mapping();
	mirror_aim();
	aim_turn_smooth();
	aim_turn_wide_deadzone();
	aim_turn_snap();
	aim_turn_untracked_gun_hand();
	aim_turn_with_middle_pinch_closed();
	prayer();
	start_pulse_outlives_the_hands();
	pinch_miss_logged();
	event_capacity_clamped();
	if (failures)
	{
		printf("hands probe: %d FAILED\n", failures);
		return 1;
	}
	printf("hands probe: OK\n");
	return 0;
}
