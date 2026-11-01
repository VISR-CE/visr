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
	/* one bone and two bones back from each tip, in the same order: what a pinch
	is judged by while the thumb is hiding the fingertip it is pinching
	(HANDS_PART_LEVEL_* below). Kept as whole families of five, as the tips are,
	so host_spatial_hands.m's reduction walks each one as a single range of
	ar_hand_skeleton_joint_name_* */
	int intermediate_tip_tracked[5];
	float intermediate_tip[5][3];
	int intermediate_base_tracked[5];
	float intermediate_base[5][3];
	int metacarpal_tracked[2];   /* index, little */
	float metacarpal[2][3];
};

/* The gesture recognizer (host_spatial_hands.c, included by
port/ios/tests/hands_probe.c): two hands' reduced joints above, turned into
the same host_spatial_controls the Sense readers fill, for
host_spatial_gamepad_map to turn into the virtual gamepad. Plain C, no
Objective-C and no host_logf: the caller logs the events this returns. */

/* a pinch's hysteresis, meters: closes below CLOSE, opens above OPEN and
holds whatever it had in between. Both are to be felt on device and are the
first thing to tune here: 0.015 / 0.025 read late to a player closing the
pinch deliberately, and 0.025 / 0.035 fired on its own from a merely relaxed
hand, so CLOSE sits between the two and OPEN is widened further still, to
make a false trigger from an unintentional near-pinch the harder of the two
mistakes to make */
#define HANDS_PINCH_CLOSE_METERS 0.018f
#define HANDS_PINCH_OPEN_METERS 0.030f
/* how far the still-visible pair behind a pinch (HANDS_PART_LEVEL_* below) must grow
apart from where it was when that pinch's fingertips were last measured before the
fingers count as parted, meters. A change and not a position on purpose: the pair sits
at whatever separation this hand and this finger put it at while the pads are touching,
which varies by player and by finger and cannot be known in advance, so the reference
is recorded live on every sample the tips themselves can be measured
(host_spatial_hands.c's part_reference_record) and only the growth needs a constant.
12 mm clears both of the things that move the pair while the pads stay in contact (a
few millimeters of joint jitter, and the centimeter or so that loosening or tightening
a grip gives) and sits well under the two centimeters and more the pair swings when the
finger and thumb really do extend out of a pinch, the finger's joint riding a proximal
phalanx around 45 mm long through tens of degrees. Half of it re-arms the carry
(hands_pinch's parted), so a pair hovering at the margin cannot chatter the trigger */
#define HANDS_PART_MARGIN_METERS 0.012f
/* how long a whole hand may go untracked before it counts as gone, seconds.
Inside this gap every pinch of that hand holds its state and the controls
those pinches drive hold with it, so a single sample in which ARKit loses the
hand is not a release (host_spatial_hands.c's hand_lost). Short on purpose:
long enough to swallow a tracking artifact of a few frames at 90 Hz, short
enough that a player lowering their hands releases the trigger at once */
#define HANDS_HAND_GAP_SECONDS 0.15f
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

/* the joints a pinch is judged by while its fingertip is hidden, most distal first: the
pinching finger's intermediate base (its middle knuckle) against the thumb's intermediate
tip, then against the thumb's intermediate base one bone further back. A pinch puts the
thumb's pad on the fingertip, so the contact point is exactly what the cameras lose, and
these are the nearest joints on either side the thumb is not sitting on. They are also the
nearest pair worth measuring: the finger's own knuckle and the thumb's hardly move against
each other at all, so a pair taken from those two could not tell a pinch from an open hand.
Both levels share the finger's joint because the thumb's is the one that goes: the thumb's
intermediate tip is a centimeter or two behind a pad the finger wraps over, while its
intermediate base sits on the outside of the hand. host_spatial_hands.c's part_decide takes
the first level both of whose ends are tracked and whose reference it has */
enum { HANDS_PART_LEVEL_THUMB_INTERMEDIATE_TIP, HANDS_PART_LEVEL_THUMB_INTERMEDIATE_BASE,
	NUMBER_OF_HANDS_PART_LEVELS };

/* one pinch's hysteresis state. carried_since is 0 while both ends of the
pinch are ARKit's own reading, and the start time of the gap in which either
end is carried (or cannot be read at all) otherwise: it times that gap for
the log and nothing else, since no deadline opens a pinch any more
(host_spatial_hands.c's pinch_update) */
struct hands_pinch
{
	int closed;
	double carried_since;
	/* the part check's own hysteresis over HANDS_PART_MARGIN_METERS: 1 once the visible
	pair has grown past the margin, back to 0 below half of it or on any sample the two
	fingertips can be measured again. While it is set no carried position may close this
	pinch, since the carry's premise -- the finger is still where it was, which is on the
	thumb -- is the thing the pair has just disproved (host_spatial_hands.c's part_decide) */
	int parted;
	/* what each level's pair measured on the last sample both of this pinch's fingertips
	were ARKit's own reading, which is the pose those tips confirmed the fingers were in.
	known is 0 where that sample could not see the level, so a reference is never carried
	over from an older pose */
	float part_reference[NUMBER_OF_HANDS_PART_LEVELS];
	int part_reference_known[NUMBER_OF_HANDS_PART_LEVELS];
};

struct hands_state
{
	struct hands_pinch pinch[2][NUMBER_OF_HANDS_PINCHES];   /* [hand][finger] */
	/* each hand's five fingertips (thumb first, as the joints above) kept in
	that hand's own palm frame: ARKit drops the outer tips constantly while
	the palm bones stay tracked, so the last position a tip was seen at is
	stored in coordinates the wrist and the two metacarpals define and read
	back through the palm's current pose, so a tip going out of sight is not by
	itself a reason to open a pinch (host_spatial_hands.c's tips_resolve; what
	does open one from behind a hidden tip is part_decide, which reads joints
	the thumb is not sitting on rather than this carry).
	known says a stored coordinate exists; a hand that stays untracked past
	its gap forgets all five, since a hand lost for that long opens every
	pinch anyway */
	float tip_local[2][5][3];
	int tip_known[2][5];
	/* when each hand's current untracked run started, 0 while ARKit is
	reporting the hand: what HANDS_HAND_GAP_SECONDS is measured against, so a
	hand that blinks holds every pinch it had and a hand that is really gone
	still opens them (host_spatial_hands.c's hand_lost) */
	double hand_gap_since[2];
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

/* PINCH_MISS is a pinch whose thumb or fingertip ARKit is not reporting, not
a pinch that opened: the reading is a carried one (or, where no carry is
possible, none at all, and the pinch holds what it had). It marks the gap's
start, and on its way out carries the gap's length as its value. Nothing but
the tip's return ends a gap now, so its threshold is 0: the measurement these
events are here for is how long the gaps run (host_spatial_hands.c's
pinch_update) */
enum { HANDS_EVENT_PINCH, HANDS_EVENT_AIM_TURN, HANDS_EVENT_PRAYER, HANDS_EVENT_START,
	HANDS_EVENT_PINCH_MISS };
/* which reading decided a pinch's event, so the log says which of this file's three paths
moved it: the two fingertips as ARKit reported them, a position carried in the palm's frame,
or the still-visible pair behind a hidden fingertip (HANDS_PART_LEVEL_*). HAND_LOST is the
whole-hand open, BLIND a gap with neither end readable at all, and NONE every other kind of
event, which has no reading of this sort */
enum { HANDS_PATH_NONE, HANDS_PATH_MEASURED, HANDS_PATH_CARRIED, HANDS_PATH_PARTED,
	HANDS_PATH_HAND_LOST, HANDS_PATH_BLIND };
struct hands_event { int kind, hand, finger, on, path; float value, threshold; };

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
/* 1 while the hands source filled the game sample it was last given: 0 while
hand tracking is unsupported, its permission pending or denied, or no
session has started it. input.aim alone decides whether this file is asked
at all (host_spatial.m) */
int host_spatial_hands_active(void);
/* zeroes the gesture recognizer's state (pinches, carried tips and their
hands' gaps, aim turn arm, pulses, the prayer sign's dwell): called whenever
hands were not this sample's source, so no pinch or dwell survives into the
next sample hands do fill (host_spatial.m's host_spatial_sample, the branch
that clears hands_filled_sample; host_spatial_hands_sample's own WAITING
gap) */
void host_spatial_hands_idle(void);
#endif

#endif
