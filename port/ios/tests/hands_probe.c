/* The gesture recognizer (port/ios/host/host_spatial_hands.c, included):
pinch hysteresis and the tip-loss hold, the gun and off hand's button and
stick mapping, the drag and aim turns (smooth and snap), the prayer sign's
dwell and Start pulse, and that a hand with tracked == 0 zeros every one of
its own controls the same sample. */
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
they need. */
static struct host_spatial_hand_joints make_hand(void)
{
	struct host_spatial_hand_joints hand;
	int i;

	memset(&hand, 0, sizeof(hand));
	hand.tracked = 1;
	hand.forearm_tracked = 1;
	for (i = 0; i < 5; i++)
	{
		hand.tip_tracked[i] = 1;
		hand.tip[i][2] = (i == 0) ? 0.0f : 0.05f;
	}
	hand.metacarpal_tracked[0] = hand.metacarpal_tracked[1] = 1;
	hand.metacarpal[0][0] = 0.03f;
	hand.metacarpal[0][2] = 0.05f;
	hand.metacarpal[1][0] = -0.03f;
	hand.metacarpal[1][2] = 0.05f;
	return hand;
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
	joints[0] = make_hand();
	joints[1] = make_hand();

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
	joints[0] = make_hand();
	joints[1] = make_hand();
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
	joints[0] = make_hand();
	joints[1] = make_hand();
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
	joints[0] = make_hand();
	joints[1] = make_hand();
	joints[0].tip[1][2] = 0.010f; /* off hand (0): index closed */
	joints[0].tip[2][2] = 0.010f; /* off hand (0): middle closed too; index wins */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == 1.0f, "off hand: index wins over middle, stick_y +1");
	check(controls[0].menu == 0, "off hand: menu never set");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[0].tip[2][2] = 0.010f; /* only middle: backward */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].stick_y == -1.0f, "off hand: middle alone, stick_y -1");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
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
	joints[0] = make_hand();
	joints[1] = make_hand();
	joints[0].tip[1][2] = 0.010f; /* hand 0 is the gun hand now */
	hands_recognize(&state, joints, 0, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[0].trigger == 1.0f, "mirror: hand 0's index fires when it is the gun hand");
	check(controls[1].trigger == 0.0f, "mirror: hand 1 (the off hand) never fires from this pinch");

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[1] = make_hand();
	joints[1].tip[1][2] = 0.010f; /* walk forward with the off hand (now hand 1) */
	hands_recognize(&state, joints, 0, forward, 1, 18.0f, 1.0, controls, events, 16);
	check(controls[1].stick_y == 1.0f, "mirror: hand 1's index walks forward as the off hand");
	check(controls[1].menu == 0, "mirror: the off hand's menu is never set, mirrored too");
}

/* the drag turn, smooth mode: deflection grows linearly between START and FULL meters of
sideways wrist travel from the pinch's close, right positive, clamped past FULL */
static void drag_smooth(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[1] = make_hand();
	joints[1].wrist[1] = 0.0f;
	joints[1].tip[2][2] = 0.010f; /* the gun hand's middle closes: the drag anchor is set here */
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.0, controls, events, 16);

	joints[1].wrist[1] = -0.085f; /* 8.5 cm to the right (right is positive lateral) */
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, 0.5f, 1e-3f), "drag smooth: 8.5 cm right is 0.5");

	joints[1].wrist[1] = 0.2f; /* 20 cm to the left, past FULL: clamped to -1 */
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_x, -1.0f, 1e-3f), "drag smooth: 20 cm left clamps to -1");
}

/* the drag turn, snap mode: one pulse per pinch past START, held for PULSE_SECONDS, no re-fire
until the pinch opens and closes again */
static void drag_snap(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	const float forward[3] = { 1.0f, 0.0f, 0.0f };
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[1] = make_hand();
	joints[1].wrist[1] = 0.0f;
	joints[1].tip[2][2] = 0.010f; /* middle closes at t = 1.0, wrist y 0 */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);

	joints[1].wrist[1] = -0.03f; /* 3 cm right, past START (2 cm) */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].stick_x == 1.0f, "drag snap: the pulse fires right");
	check(count == 1 && events[0].kind == HANDS_EVENT_DRAG && events[0].on == 1, "drag snap: one drag event");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.14, controls, events, 16); /* t + 0.09 */
	check(controls[1].stick_x == 1.0f, "drag snap: the pulse still holds at t + 0.09");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.16, controls, events, 16); /* t + 0.11 */
	check(controls[1].stick_x == 0.0f, "drag snap: the pulse has released by t + 0.11");

	joints[1].wrist[1] = -0.1f; /* still dragging further in the same pinch: no second pulse */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.2, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "drag snap: no re-fire while the same pinch stays closed");

	joints[1].tip[2][2] = 0.05f; /* reopen */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.3, controls, events, 16);
	joints[1].tip[2][2] = 0.010f; /* and close again, at a fresh anchor */
	joints[1].wrist[1] = -0.03f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.4, controls, events, 16);
	joints[1].wrist[1] = -0.06f;
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.45, controls, events, 16);
	check(controls[1].stick_x == 1.0f, "drag snap: a fresh close pinch fires again");
	check(controls[0].menu == 0, "drag snap: the off hand's menu is never set");
}

/* the aim turn, smooth mode, suppressed while the middle pinch is closed: deflection grows
linearly from the deadzone edge to FULL degrees, a left yaw turning left (negative stick x) */
static void aim_turn_smooth(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[1] = make_hand();

	forward[0] = cosf(10.0f * (float)M_PI / 180.0f); forward[1] = sinf(10.0f * (float)M_PI / 180.0f); forward[2] = 0.0f;
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.0, controls, events, 16);
	check(controls[1].stick_x == 0.0f, "aim smooth: 10 degrees left is inside the deadzone");

	forward[0] = cosf(31.5f * (float)M_PI / 180.0f); forward[1] = sinf(31.5f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.1, controls, events, 16);
	check(near(controls[1].stick_x, -0.5f, 1e-3f), "aim smooth: 31.5 degrees left is -0.5");

	forward[0] = cosf(-45.0f * (float)M_PI / 180.0f); forward[1] = sinf(-45.0f * (float)M_PI / 180.0f);
	hands_recognize(&state, joints, 1, forward, 0, 18.0f, 1.2, controls, events, 16);
	check(near(controls[1].stick_x, 1.0f, 1e-3f), "aim smooth: 45 degrees right is +1");
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
	joints[0] = make_hand();
	joints[1] = make_hand();

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

/* the aim turn is suppressed while the gun hand's middle pinch is closed, even well past the
deadzone: the drag is the only source of stick_x (Review Focus 4) */
static void aim_turn_suppressed_by_drag(void)
{
	struct hands_state state;
	struct host_spatial_hand_joints joints[2];
	struct host_spatial_controls controls[2];
	struct hands_event events[16];
	float forward[3];
	int count;

	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[1] = make_hand();
	forward[0] = cosf(40.0f * (float)M_PI / 180.0f); forward[1] = sinf(40.0f * (float)M_PI / 180.0f); forward[2] = 0.0f;

	joints[1].wrist[1] = 0.0f;
	joints[1].tip[2][2] = 0.010f; /* the gun hand's middle closes: drag owns the stick now */
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.0, controls, events, 16);

	joints[1].wrist[1] = -0.03f; /* past the drag's START, at yaw 40 (well past the deadzone) */
	count = hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.05, controls, events, 16);
	check(controls[1].stick_x == 1.0f, "suppressed: the drag's pulse alone sets stick_x");
	check(count == 1, "suppressed: exactly one pulse, from the drag");
	check(events[0].kind == HANDS_EVENT_DRAG, "suppressed: the one pulse is the drag's, not the aim turn's");
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

	/* hand 0 (left) and hand 1 (right), palms opposed per the pinned formula:
	normal = normalize(cross(index_mc - wrist, little_mc - wrist)); swapping which metacarpal
	is "index" between the two hands gives opposed normals without needing a measured sign */
	memset(&state, 0, sizeof(state));
	joints[0] = make_hand();
	joints[1] = make_hand();

	joints[0].wrist[1] = 0.05f;
	joints[0].metacarpal[0][0] = 0.03f; joints[0].metacarpal[0][1] = 0.05f; joints[0].metacarpal[0][2] = 0.05f;
	joints[0].metacarpal[1][0] = -0.03f; joints[0].metacarpal[1][1] = 0.05f; joints[0].metacarpal[1][2] = 0.05f;

	joints[1].wrist[1] = -0.05f;
	joints[1].metacarpal[0][0] = -0.03f; joints[1].metacarpal[0][1] = -0.05f; joints[1].metacarpal[0][2] = 0.05f;
	joints[1].metacarpal[1][0] = 0.03f; joints[1].metacarpal[1][1] = -0.05f; joints[1].metacarpal[1][2] = 0.05f;

	/* a 1.0 baseline keeps every timestamp strictly positive, away from the state's own
	zero-means-idle sentinels (prayer_since, pulse/cooldown clocks) */
	for (t = 1.0; t <= 1.55 + 1e-9; t += 0.05)
		hands_recognize(&state, joints, 1, forward, 1, 18.0f, t, controls, events, 16);
	check(controls[1].menu == 0, "prayer: not yet at 0.55 s held");

	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.6, controls, events, 16);
	check(controls[1].menu == 1, "prayer: fires at 0.6 s held");
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.69, controls, events, 16);
	check(controls[1].menu == 1, "prayer: Start still held at 0.69 s");
	hands_recognize(&state, joints, 1, forward, 1, 18.0f, 1.71, controls, events, 16);
	check(controls[1].menu == 0, "prayer: Start released by 0.71 s");

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

int main(void)
{
	pinch_hysteresis();
	pinch_loss_hold();
	hand_untracked_zeros();
	off_hand_mapping();
	mirror_aim();
	drag_smooth();
	drag_snap();
	aim_turn_smooth();
	aim_turn_snap();
	aim_turn_suppressed_by_drag();
	prayer();
	if (failures)
	{
		printf("hands probe: %d FAILED\n", failures);
		return 1;
	}
	printf("hands probe: OK\n");
	return 0;
}
