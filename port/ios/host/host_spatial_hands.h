/* Bare hands as the aim source (input.aim = "hands"): host_spatial_hands.m
fills the same structs the Sense controllers fill (halo_spatial.h), so the
guest's aim path (controller_aim_game.c) sees one kind of hand.

Split on purpose: the reduced joints below are plain C, so a host-side test
can check what reads them (as host_spatial_gamepad.h is), and only the two
samples and the source flag need CompositorServices and Objective-C. */
#ifndef HOST_SPATIAL_HANDS_H
#define HOST_SPATIAL_HANDS_H

#include "halo_spatial.h"
#include "host_spatial_gamepad.h"

/* one hand's joints, reduced for the recognizer: positions in the room's
level frame turned into the game's axes, meters from the head, exactly as
halo_spatial_location's position is (host_spatial_pose.h) */
struct host_spatial_hand_joints
{
	int tracked;                 /* the anchor and its wrist joint; nothing
		in this struct is usable without the wrist */
	int forearm_tracked;
	float wrist[3], forearm_arm[3];
	int tip_tracked[5];          /* thumb, index, middle, ring, little */
	float tip[5][3];
	int metacarpal_tracked[2];   /* index, little */
	float metacarpal[2][3];
};

/* The gesture recognizer (host_spatial_hands.c, included by
port/ios/tests/hands_probe.c): two hands' reduced joints above, turned into
the same host_spatial_controls the Sense readers fill, for
host_spatial_gamepad_map to turn into the virtual gamepad. Plain C, no
Objective-C and no host_logf: the caller logs the events this returns. */

/* a pinch's hysteresis, meters: closes below CLOSE, opens above OPEN; a
tip lost mid-pinch holds its state for HOLD_SECONDS before opening */
#define HANDS_PINCH_CLOSE_METERS 0.015f
#define HANDS_PINCH_OPEN_METERS 0.025f
#define HANDS_PINCH_HOLD_SECONDS 0.2
/* the aim turn's ramp, degrees of arm past the deadzone to full deflection. A span, not an
absolute angle: the deadzone itself is input.hand_turn_deadzone, anywhere from 5 to 60
degrees, and a ramp measured from 0 would divide by a negative span (inverting the turn) or
by zero for the wider half of that range. 27 puts the default deadzone's full deflection at
18 + 27 = 45 degrees of arm */
#define HANDS_AIM_TURN_RAMP_DEGREES 27.0f
/* the prayer sign's geometry and timing, measured on device (see the
comment on palm_frame in host_spatial_hands.c for the measurement) */
#define HANDS_PRAYER_DISTANCE_METERS 0.16f
#define HANDS_PRAYER_OPPOSED_DOT -0.4f
#define HANDS_PRAYER_FINGERS_UP 0.5f
#define HANDS_PRAYER_DWELL_SECONDS 0.6
#define HANDS_PRAYER_COOLDOWN_SECONDS 0.5
/* how long a snap turn's pulse or the prayer sign's Start pulse holds its
value on the clock hands_recognize is given, so the caller's sampler (not a
call count) decides how many samples see it */
#define HANDS_PULSE_SECONDS 0.1

enum { HANDS_PINCH_INDEX, HANDS_PINCH_MIDDLE, HANDS_PINCH_RING, HANDS_PINCH_LITTLE, NUMBER_OF_HANDS_PINCHES };

/* one pinch's hysteresis state; lost_since is 0 while tracked and closing
normally, the untracked gap's start time while timing toward HOLD_SECONDS,
and a negative sentinel once the pinch has opened because of a loss (the
hand's or the tip's): that sentinel blocks closing again until a tracked
sample reads the distance above OPEN, so a dropped tip never re-fires on
return (host_spatial_hands.c's pinch_update) */
struct hands_pinch { int closed; double lost_since; };

struct hands_state
{
	struct hands_pinch pinch[2][NUMBER_OF_HANDS_PINCHES];   /* [hand][finger] */
	int aim_turn_armed;                                      /* snap mode: inside the deadzone since the last snap */
	/* smooth mode: the turn's contribution is nonzero, so the log gets the
	same on and off transitions snap mode's pulse gives it */
	int aim_turning;
	double pulse_until; float pulse_x;                       /* the right stick's held snap value */
	int pulse_kind;                                          /* the aim turn, for its own off event */
	/* the prayer sign's geometry gate (distance, fingers) on its own, independent of the
	palm normals' unmeasured sign: its own on/off transition is what carries the measured dot
	in the log (host_spatial_hands.c's prayer_near) */
	int prayer_near;
	double prayer_since; int prayer_fired; double prayer_cooldown_until;
	double start_until;                                      /* the Start pulse */
};

/* PINCH_MISS is a pinch nothing could read, not a pinch that opened: the
thumb or the fingertip went untracked, which is the one reason a deliberate
pinch does nothing at all, and its value is the gap's length against the
hold that ends it (host_spatial_hands.c's pinch_update) */
enum { HANDS_EVENT_PINCH, HANDS_EVENT_AIM_TURN, HANDS_EVENT_PRAYER, HANDS_EVENT_START,
	HANDS_EVENT_PINCH_MISS };
struct hands_event { int kind, hand, finger, on; float value, threshold; };

/* one sample: the two hands' joints (index 0 left, 1 right), which is the
gun hand, the gun hand's aim forward in the game's axes (from
halo_spatial_hand.aim), snap or smooth turning, the deadzone, the time;
fills both controls (index 0 left, 1 right) for host_spatial_gamepad_map,
and up to `capacity` state-change events for the caller to log. Returns
the event count, clamped to `capacity`: a caller's events[0..count) is
always in bounds, even when a sample changes more states than `capacity`
holds. Never sets other->menu: there is no off-hand gesture for it, and
host_spatial_gamepad_map reads that field as the pad's Back */
int hands_recognize(struct hands_state *state, const struct host_spatial_hand_joints joints[2],
	int aiming_hand, const float aim_forward[3], int turn_snap, float deadzone_degrees, double now,
	struct host_spatial_controls controls[2], struct hands_event *events, int capacity);

#ifdef __OBJC__
#import <CompositorServices/CompositorServices.h>

/* the drawing sample's hands, queried at predict_to (the drawable's
trackable-anchor time, the instant the Sense tracker predicts to) and held
for the drawing sample that follows in this frame; a NULL drawable drops the
held one, as host_spatial_frame's does. The hold through a pending recenter
is the caller's instead: host_spatial.m's hands_frame returns without calling
in at all, drawable or not, so the held sample stays paired with the head it
was made against until the origin rebases */
void host_spatial_hands_frame(cp_drawable_t drawable, const float head_position[3], double predict_to);
/* fills the sample's hands, reason and (game sample) the gamepad from the
latest hand anchors; returns 1 when it filled, 0 when hands are not the
source this sample (unsupported, off, no session) */
int host_spatial_hands_sample(int kind, struct halo_spatial_sample *sample, const float head_position[3]);
/* 1 while the hands source filled the game sample it was last given. The
Sense tracker wins while it is READY and then this is never asked, so
host_spatial.m's precedence, not this flag, decides who owns a sample */
int host_spatial_hands_active(void);
/* zeroes the gesture recognizer's state (pinches, aim turn arm, pulses, the
prayer sign's dwell): called whenever hands were not this
sample's source, so no pinch or dwell survives into the next sample hands do
fill (host_spatial.m's host_spatial_sample, the branch that clears
hands_filled_sample; host_spatial_hands_sample's own WAITING gap) */
void host_spatial_hands_idle(void);
#endif

#endif
