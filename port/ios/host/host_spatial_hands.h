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
	/* one, two and three bones back from each tip, in the same order: what a pinch
	is judged by while the thumb is hiding the fingertip it is pinching
	(HANDS_PART_LEVEL_* below). Kept as whole families of five, as the tips are,
	so host_spatial_hands.m's reduction walks each one as a single range of
	ar_hand_skeleton_joint_name_* */
	int intermediate_tip_tracked[5];
	float intermediate_tip[5][3];
	int intermediate_base_tracked[5];
	float intermediate_base[5][3];
	int knuckle_tracked[5];
	float knuckle[5][3];
	int metacarpal_tracked[2];   /* index, little */
	float metacarpal[2][3];
};

/* The gesture recognizer (host_spatial_hands.c, included by
port/ios/tests/hands_probe.c): two hands' reduced joints above, turned into
the same host_spatial_controls the Sense readers fill, for
host_spatial_gamepad_map to turn into the virtual gamepad. Plain C, no
Objective-C and no host_logf: the caller logs the events this returns. */

/* a pinch's hysteresis: closes below CLOSE, opens above OPEN and holds whatever it had in
between. Both are fractions of a ruler measured from the player's own hand rather than
absolute meters, because a session of raw distances (debug.hand_distance_log, whose line is
host_spatial_hands.m's periodic log) finally said where the two absolute numbers were sitting,
and it was inside the wrong distribution. For the aiming hand, 3690 samples:

	a deliberate pinch, n=417:   min 0.0010  25th 0.0051  median 0.0071  75th 0.0094
	                             95th 0.0206  max 0.0287
	a resting hand,     n=1236:  min 0.0183  5th 0.0242  25th 0.0319  median 0.0391
	                             75th 0.0474  95th 0.0655  max 0.1110

The old CLOSE of 0.018 sat a fifth of a millimeter under that resting minimum, so a merely
relaxed hand reached it and fired, and the old OPEN of 0.030 sat inside the resting lower
quartile, so a tap's release waited while the hand settled on top of it. Five rounds of work
read that second number as a fault in the release rules and tuned those instead (the carry's
deadline, the joints part_pair_of watches, the growth part_decide asks for): the rules were
answering correctly about a threshold in the wrong place.

The ruler is a per-finger measure so the thresholds scale with the player's own fingers, which
is what a joint radius would do if ARKit reported one. It does not: "radius" appears nowhere in
the XROS 27 ARKit headers, and ar_hand_skeleton_get_joint_named hands back a joint with a
transform and a tracked flag and nothing about its size. So the stand-in is the finger's distal
segment, its intermediate tip to its own tip, which the same session measured at 0.0224 for the
index finger with a spread under 1 mm across the whole of it: the steadier of the two rulers
that session logged (palm width varied by about 2 mm), and per-finger where palm width is one
number for the hand.

Two thirds of that segment for CLOSE and one whole segment for OPEN puts this player's band at
0.0149 and 0.0224, which is the clear air between his two distributions: CLOSE 3.4 mm below the
resting minimum of 0.0183, so no relaxed hand reaches it, and OPEN 1.8 mm above the pinching
95th percentile of 0.0206, so no held pinch drifts over it. A bigger hand's band scales with its
own fingers rather than keeping this one's millimeters */
#define HANDS_PINCH_CLOSE_RULER_FRACTION (2.0f / 3.0f)
#define HANDS_PINCH_OPEN_RULER_FRACTION 1.0f
/* the ruler to use until this finger has been measured enough times to trust: the index
finger's segment as the session above measured it. Its only job is to be a real number of the
right order, so that a hand whose joints never arrive keeps a working band instead of
collapsing both thresholds to 0, and so the first samples after the hands become the aim source
are not judged against nothing. All four fingers fall back to it: it is the one ruler that was
actually measured, and host_spatial_hands.c's own per-finger window replaces it within
HANDS_RULER_MINIMUM_SAMPLES below, which is a fraction of a second of an open hand */
#define HANDS_RULER_DEFAULT_METERS 0.0224f
/* the range a reading has to land in to count as a finger at all. A distal segment of a human
finger runs from about 10 mm (a small little finger) to about 40 mm (a large index), and a
reading outside that is a tracking glitch and not a measurement: host_spatial_hands.c's
ruler_push drops it rather than folding it in, so no glitch can move a threshold even by the
boundary's worth. These two are also what keeps a threshold away from 0 from the other
direction, since the window can only ever hold values between them */
#define HANDS_RULER_MINIMUM_METERS 0.010f
#define HANDS_RULER_MAXIMUM_METERS 0.040f
/* how many readings each finger's ruler is smoothed over, and how few it may be trusted on.
A median over a window and not one sample: the segment is a constant of the player's hand
rather than a signal (under 1 mm of spread over 3690 samples), so the window is there to throw
a glitch away, not to track a change, and a median of fifteen needs eight bad readings running
before it moves at all where a mean would be dragged by one. Fifteen is about a sixth of a
second at 90 Hz, which is as much history as a constant needs; five is 56 ms, short enough that
the default above is gone almost as soon as a hand is in view and long enough that no single
reading is ever the ruler on its own */
#define HANDS_RULER_SAMPLES 15
#define HANDS_RULER_MINIMUM_SAMPLES 5
/* how far the still-visible pair behind a pinch (HANDS_PART_LEVEL_* below) must grow
apart from where it was when that pinch's fingertips were last measured before the
fingers count as parted, meters. A change and not a position on purpose: the pair sits
at whatever separation this hand and this finger put it at while the pads are touching,
which varies by player and by finger and cannot be known in advance, so the reference
is recorded live on every sample the tips themselves can be measured
(host_spatial_hands.c's part_reference_record) and only the growth needs a constant.
Half of it re-arms the carry (hands_pinch's parted), so a pair hovering at the margin
cannot chatter the trigger.

5 mm is provisional, and it is deliberately low: the first value here was 12 mm,
reasoned from the length of a proximal phalanx and the angle it swings through, and a
whole device session released one pinch by this path. Every other release waited for the
hidden fingertip to come back, which is the defect the check exists to fix. So the
number stops being argued and starts being measured: HANDS_EVENT_PART_GROWTH below
reports the pair's growth at every release the fingertips themselves decided, whatever
that growth was, so one session of ordinary taps and holds gives the distribution of
growths at a release the player meant. This constant becomes the low end of that
distribution, with the jitter floor the same session shows under it. Until then 5 mm is
chosen to be smaller than any real parting rather than larger than any noise, with
HANDS_PART_CONFIRM_SAMPLES below carrying the noise instead */
#define HANDS_PART_MARGIN_METERS 0.005f
/* how many samples running the pair must be past that margin before a pinch is released.
2, which costs the release one sample (11 ms at 90 Hz, under a tenth of the time the
plasma pistol takes to charge) and is what makes the small margin safe to hold a trigger
against. One sample would not be: a single noisy reading past the margin sets
hands_pinch's parted, and parted does not clear again until the growth falls under half
the margin or the fingertips can be measured again, neither of which a hand that is
still pinching with the tip hidden will do. So one spike does not cost a sample of
trigger, it costs the rest of the hold, and at 90 Hz a hold of a few seconds is
thousands of chances to take that spike. Requiring the parting twice running makes a
false release need two spikes in a row, while a real parting only grows further on the
sample after it starts and so always confirms. It is a count and not a clock: nothing
about it accumulates over a hold (host_spatial_hands.c's part_decide) */
#define HANDS_PART_CONFIRM_SAMPLES 2
/* how long a whole hand may go untracked before it counts as gone, seconds.
Inside this gap every pinch of that hand holds its state and the controls
those pinches drive hold with it, so a single sample in which ARKit loses the
hand is not a release (host_spatial_hands.c's hand_lost). Short on purpose:
long enough to swallow a tracking artifact of a few frames at 90 Hz, short
enough that a player lowering their hands releases the trigger at once.

This gap is the pinches' alone. The aim has its own, input.hand_aim_gap,
decided by hands_aim_hold below: the two cover the same blink but answer to
different costs, so the aim's is a setting and this one is a constant */
#define HANDS_HAND_GAP_SECONDS 0.15f
/* the aim turn's ramp, degrees of arm past the deadzone to full deflection. A span, not an
absolute angle: the deadzone itself is input.hand_turn_deadzone, anywhere from 5 to 60
degrees, and a ramp measured from 0 would divide by a negative span (inverting the turn) or
by zero for the wider half of that range. 27 puts the default deadzone's full deflection at
18 + 27 = 45 degrees of arm.

The turn's alone. The arm's elevation shared this span and input.hand_turn_deadzone with it
when the vertical axis was added, on the reasoning that one arm swinging through one range
should feel alike on both axes of the one stick, with the note that they should be split once
a device session said otherwise. It said otherwise: the turn is liked as it is and the
elevation reads as slow, because 18 + 27 asks for the arm 45 degrees above its corrected zero
for a full deflection and nobody's comfortable vertical reach is as long as their sideways
sweep. So the elevation takes input.hand_elevation_deadzone and input.hand_elevation_span
instead, which arrive as hands_recognize's own arguments (as the turn's deadzone already did)
and default to 9 and 14. This constant stays a constant because only the turn reads it now and
nothing has asked for the turn to change */
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

/* the joints a pinch is judged by while its fingertip is hidden, as a ladder of pairs the
finger's end steps down and the thumb's end steps along, most distal first. A pinch puts the
thumb's pad on the fingertip, so the contact point is exactly what the cameras lose, and
every level here is a pair the thumb is not sitting on.

The finger's end is named first because it is the end that has to move. A finger opening out
of a pinch rotates almost entirely about its own two outer joints: the fingertip travels
centimeters, the joint right behind it nearly as far, and the middle knuckle (the
intermediate base) barely translates at all. A device session that watched the middle
knuckle released one pinch in the whole session, which is what the finger's intermediate tip
is here to fix: it sits a bone behind the contact point, out of the thumb's shadow, and
still travels with the tip. Below it the ladder gives up signal for visibility, the
intermediate base first and the knuckle last, since a pair taken off the knuckle can only
just tell a pinch from an open hand and is the last thing worth asking.

The thumb's end steps from its intermediate tip to its intermediate base within each of the
finger's: the finger's distal phalanx wraps over the thumb's pad, so the thumb's
intermediate tip is the first joint of the two at risk, while its intermediate base sits on
the outside of the hand. Stepping the thumb before the finger is on purpose, since losing
the thumb's more distal joint costs far less of the pair's travel than giving up the
finger's.

host_spatial_hands.c's part_decide takes the first level both of whose ends are tracked and
whose reference it has */
enum { HANDS_PART_LEVEL_INTERMEDIATE_TIP_THUMB_TIP, HANDS_PART_LEVEL_INTERMEDIATE_TIP_THUMB_BASE,
	HANDS_PART_LEVEL_INTERMEDIATE_BASE_THUMB_TIP, HANDS_PART_LEVEL_INTERMEDIATE_BASE_THUMB_BASE,
	HANDS_PART_LEVEL_KNUCKLE_THUMB_TIP, HANDS_PART_LEVEL_KNUCKLE_THUMB_BASE,
	NUMBER_OF_HANDS_PART_LEVELS };

/* one pinch's hysteresis state. carried_since is 0 while both ends of the
pinch are ARKit's own reading, and the start time of the gap in which either
end is carried (or cannot be read at all) otherwise: it times that gap for
the log and nothing else, since no deadline opens a pinch any more
(host_spatial_hands.c's pinch_update) */
struct hands_pinch
{
	int closed;
	/* when this pinch closed, 0 while it is open: what every release reports as the length of
	the hold it ended (HANDS_EVENT_PINCH_HELD below, host_spatial_hands.c's held_ended). It is
	set on the close transition alone, so a state zeroed under a closed pinch (whenever hands
	were not a sample's source: host_spatial_hands_idle) drops that hold without a line rather
	than measuring it from 0 */
	double closed_since;
	double carried_since;
	/* the part check's own hysteresis over HANDS_PART_MARGIN_METERS: 1 once the visible
	pair has grown past the margin, back to 0 below half of it or on any sample the two
	fingertips can be measured again. While it is set no carried position may close this
	pinch, since the carry's premise -- the finger is still where it was, which is on the
	thumb -- is the thing the pair has just disproved (host_spatial_hands.c's part_decide) */
	int parted;
	/* how many samples running the pair has been past the margin, counting up to
	HANDS_PART_CONFIRM_SAMPLES and stopping there: what keeps a single noisy reading from
	setting parted, which no hand still holding a pinch could clear again. Any sample inside
	the margin puts it back to 0, as does a sample the fingertips themselves can be measured
	on (part_reference_record) and the loss of the whole hand */
	int part_over;
	/* what each level's pair measured on the last sample both of this pinch's fingertips
	were ARKit's own reading, which is the pose those tips confirmed the fingers were in.
	known is 0 where that sample could not see the level, so a reference is never carried
	over from an older pose */
	float part_reference[NUMBER_OF_HANDS_PART_LEVELS];
	int part_reference_known[NUMBER_OF_HANDS_PART_LEVELS];
	/* this finger's own ruler, as a ring buffer of its last readings and a count of how many
	of them there are: the window host_spatial_hands.c's ruler_meters takes the median of to
	scale HANDS_PINCH_CLOSE_RULER_FRACTION and HANDS_PINCH_OPEN_RULER_FRACTION above. Readings
	come only from a sample where ARKit reports both ends of the segment itself, with no part
	for the carried tip above, so a pinch (which hides the fingertip it is measuring) simply
	stops filling the window and keeps the open hand's readings for the length of the hold.
	Valid entries are ruler_sample[0..ruler_count), ruler_next being where the next one lands.

	It is the one thing in this struct that outlives a lost hand. Everything else here is a
	reading of a pose and is cleared when the hand goes (host_spatial_hands.c's pinch_update,
	its !hand_tracked branch), but a finger's length is a fact about the player: the hand that
	comes back has the fingers that left. Only host_spatial_hands_idle's zeroing of the whole
	state forgets it, and there the five samples it takes to rebuild are spent while the hands
	are becoming the aim source anyway */
	float ruler_sample[HANDS_RULER_SAMPLES];
	int ruler_next, ruler_count;
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
	/* the same for the arm's elevation, on both turn settings: the elevation
	has no snap mode to report itself instead (hands_recognize) */
	int aim_elevating;
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
/* PART_GROWTH is not a state change at all: it is the margin's own measurement, emitted
alongside every pinch release the two fingertips themselves decided (host_spatial_hands.c's
pinch_update). Its value is how far the still-visible pair behind the pinch
(HANDS_PART_LEVEL_*) had grown from its reference at that instant and its threshold is
HANDS_PART_MARGIN_METERS, so a session of ordinary taps and holds gives the distribution of
growths at a real release and the margin stops being a guess. It is emitted whatever that
growth was, including the growths well under the margin, which are the ones worth seeing.
`on` says whether the release came out of a carried or blind gap, so the growth spans that
gap and is the number the part check would have had to act on; off means the tips were
ARKit's own reading on the previous sample too, which makes the growth one sample wide and
not a measurement of anything. Its path is MEASURED where a level was readable and BLIND
where none was, which is the one case the part check cannot act at all */
/* PINCH_HELD is the one thing the pinch work has never measured: how long the trigger was
actually held. Three rounds of tuning have each moved a proxy for a prompt release (the
carry's deadline, then the joints the part check watches, then the growth it asks for) while
the complaint stayed the same, that a quick tap becomes a hold, and nothing in this file ever
reported the duration being complained about. So one line comes out of every release, on
every path that can release a pinch: its value is how many seconds that pinch had been
closed, and its own path is the release's, so a session of ordinary play sorts into how long
a tap really lasts and which reading ended it. Its threshold is 0, as PINCH_MISS's is and for
the same reason: no duration decides anything here, the number is the measurement itself. `on`
is 0 because the pinch is open by the time the line is written. Instrumentation only: no
decision anywhere in this file reads closed_since */
/* AIM_ELEVATION is the aiming stick's vertical axis as the turn's events are its horizontal
one: on as the arm's elevation first carries the stick off zero, off as it falls back, with
the elevation it measured against the deadzone it crossed, which is
input.hand_elevation_deadzone and not the turn's. Unlike the turn it has no snap
mode, so it reports on both settings of input.hand_turn_snap
(host_spatial_hands.c's hands_recognize) */
enum { HANDS_EVENT_PINCH, HANDS_EVENT_AIM_TURN, HANDS_EVENT_PRAYER, HANDS_EVENT_START,
	HANDS_EVENT_PINCH_MISS, HANDS_EVENT_PART_GROWTH, HANDS_EVENT_AIM_ELEVATION,
	HANDS_EVENT_PINCH_HELD };
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
halo_spatial_hand.aim, whose index 2 is up, so the aiming stick's turn comes
off its yaw and its elevation off its rise), snap or smooth turning, the
turn's deadzone (input.hand_turn_deadzone), the elevation's own deadzone and
span (input.hand_elevation_deadzone and input.hand_elevation_span, degrees;
the turn's span is HANDS_AIM_TURN_RAMP_DEGREES above and is not a setting),
input.aim_pitch_offset (degrees, the same trim
controller_aim_game.c applies to the barrel ray), the time;
fills both controls (index 0 left, 1 right) for host_spatial_gamepad_map,
and up to `capacity` state-change events for the caller to log. Returns
the event count, clamped to `capacity`: a caller's events[0..count) is
always in bounds, even when a sample changes more states than `capacity`
holds. Never sets other->menu: there is no off-hand gesture for it, and
host_spatial_gamepad_map reads that field as the pad's Back */
int hands_recognize(struct hands_state *state, const struct host_spatial_hand_joints joints[2],
	int aiming_hand, const float aim_forward[3], int turn_snap, float deadzone_degrees,
	float elevation_deadzone_degrees, float elevation_span_degrees,
	float pitch_offset_degrees, double now,
	struct host_spatial_controls controls[2], struct hands_event *events, int capacity);

/* debug.hand_distance_log alone: where one of the four distances below came from. Not
HANDS_PATH_*, which is which reading moved a pinch's *event* and has PARTED, HAND_LOST and
BLIND to say so a decision was made; a raw distance decides nothing, so it only needs to say
whether both of its ends this sample are ARKit's own reading, one or both carried in the
palm's frame (tips_resolve in host_spatial_hands.c), or neither readable at all */
enum { HANDS_DISTANCE_MEASURED, HANDS_DISTANCE_CARRIED, HANDS_DISTANCE_UNAVAILABLE };

/* one hand's thumb-to-fingertip distances and scale references, for debug.hand_distance_log
alone (host_spatial_hands.m's periodic log), nothing here read by any decision in this file.
distance and path are indexed as every other per-finger array here is (HANDS_PINCH_INDEX
first; hands_tip_distances below). palm_width is the index metacarpal to the little
metacarpal, and index_distal the index finger's intermediate tip to its own tip: two
measurements of this hand's own size, logged so that a session could say which of them was
steady enough to put the pinch hysteresis in a fraction of, rather than in an absolute
distance that fits one player's hand and not another's. index_distal won that comparison and
is now the ruler (HANDS_PINCH_CLOSE_RULER_FRACTION above), measured per finger rather than for
the index alone; both stay logged, since palm width is what says the hand is being seen at the
right scale at all. Both read straight off the joints with no carry of their own, since
neither is a fingertip a thumb ever hides; *_known is 0 where ARKit is not reporting the joint
it needs this sample, and the measurement is then 0 too */
struct hands_tip_distances
{
	float distance[NUMBER_OF_HANDS_PINCHES];
	int path[NUMBER_OF_HANDS_PINCHES];
	float palm_width;
	int palm_width_known;
	float index_distal;
	int index_distal_known;
};

/* this hand's four thumb-to-fingertip distances and its two scale references as this
sample's own joints and tips_resolve would read them. May write state->tip_local and
state->tip_known exactly as hands_recognize's own call into tips_resolve already did for
this sample (the carry's write, not a decision), so calling this after hands_recognize has
run for the same state and the same joints changes nothing that call did not already
change. Called only for debug.hand_distance_log */
void hands_tip_distances(struct hands_state *state, int hand, const struct host_spatial_hand_joints *joints,
	struct hands_tip_distances *out);

/* One hand's aim through a dropout: whether this sample's aim is the hand's own reading, the
last one held over, or nothing at all. Plain C and no ARKit, so hands_probe.c can check the
three answers; host_spatial_hands.m's hand_of keeps the pose and does the holding.

Why a hold here at all. The aim had none: the first sample in which the hand anchor, its
skeleton or its wrist joint is missing used to reach the guest as HALO_SPATIAL_UNTRACKED,
where the pinches on the same hand already got HANDS_HAND_GAP_SECONDS to ride the same blink
out. That asymmetry is expensive out of all proportion to the blink, because of what the
guest does with a loss rather than anything here: a recovery is gated on a dwell that
lengthens with every loss soon after the last, and the aim keeps easing toward the head for
as long as that dwell makes it wait. Measured in port/ios/tests/controller_aim_probe.c's
brief_loss_reaches_the_head: by the third loss inside five seconds the dwell is a whole
second, and a dropout of a single sample costs about 300 ms of head aim and a 200 ms ease
back, which is the snap a player sees and reports.

So a blink shorter than the gap never reaches the guest at all, and the dwell never starts
climbing. That is the property worth having, and it does not depend on knowing the blink
lengths in advance: whatever they are, the ones inside the gap cost nothing, and a hand
genuinely gone for longer still hands the aim over on the sample the gap runs out, exactly as
it did before this existed.

readable is 1 on a sample this hand's own joints could build an aim from, which is the only
thing that ends a gap. gap_seconds is input.hand_aim_gap, already clamped by the caller; at 0
every unreadable sample is LOST on the spot, which is the behaviour this replaced, and a NaN
answers the same way rather than holding forever. HELD needs a pose to hold, so the first
unreadable samples of a session are LOST however long the gap is (hold->known) */
enum { HANDS_AIM_GOOD, HANDS_AIM_HELD, HANDS_AIM_LOST };

/* one hand's hold. known says a readable sample has been seen, so there is a pose to hold;
lost_since is the start of the current unreadable run, 0 while the hand is being read. Zeroed
is the right starting state, and zeroed again whenever hands stop being the aim source, so no
run is ever measured across a gap in which this file was not being asked */
struct hands_aim_hold { int known; double lost_since; };

int hands_aim_hold(struct hands_aim_hold *hold, int readable, float gap_seconds, double now);

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
