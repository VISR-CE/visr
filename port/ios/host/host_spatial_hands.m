/* Bare hands for hand aim (input.aim = "hands"): the second source of the
same struct halo_spatial_hand the PlayStation VR2 Sense controllers fill
(host_spatial.m, halo_spatial.h). Everything downstream is unchanged -- the
sightline, the IK arms, the laser and the reticle all read hands[aiming_hand]
and never ask where it came from (controller_aim_game.c).

Two samples, the same two host_spatial.m takes. The game sample reads the
provider's latest anchors; the drawing sample (host_spatial_hands_frame,
inside host_stereo_frame) queries both anchors at the drawable's
trackable-anchor time, the instant the Sense tracker predicts its own
controllers to, and holds them for the drawing sample that follows in the
same frame.

ARKit is read on the game thread only. The permission's handler runs on an
arbitrary queue and the provider's update handler on this file's own serial
queue, and neither touches anything but the locked queue of changes the game
thread drains, exactly as host_spatial.m's header describes. The update
handler is empty: routine anchor updates are off without one, and the samples
fill the two anchors themselves. Those two anchors are made once, because the
provider fills anchors the caller owns rather than handing out a collection.

A hand anchor carries a skeleton whose joints hang off the wrist, not an aim
location, so the aim is built rather than read: its position is the wrist,
its barrel the forearm's direction (the wrist less the forearm's arm joint)
and its up the room's up squared off against that barrel, as a room transform
that host_spatial_location turns into the room's level frame exactly as it
turns a controller's aim location (host_spatial_pose.h). Nothing of the
hand's own orientation reaches the aim, which is what keeps the gun upright
under a hand that rolls freely about the forearm it is aiming with.
The game sample also runs the gesture recognizer (host_spatial_hands.c,
plain C over the reduced joints below) and host_spatial_gamepad_map onto the
same virtual gamepad the Sense pair fills; it marks itself gamepad_valid 2,
which marks the sample as the hands source's. xinput_sdl.c merges that into
port 0 like a Sense sample and remaps it differently in menus.

Hands sit outside host_spatial.m's visionOS 27 gates. Accessory tracking is
only complete in the 27 SDK (HOST_SPATIAL_SENSE_SDK) and the Sense tracker is
gated on it twice over; hand tracking and the rendering-corrected transform
getters are visionOS 1.0 and 2.0 API, so this file needs neither gate and
runs wherever the immersive path does (@available(visionOS 26.0, *), as
host_theater.m). */
#include "host_spatial_hands.h"
#include "host_spatial_pose.h"
#include "host_stereo.h"
#include "host_config.h"
#include "host.h"
#include <TargetConditionals.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if TARGET_OS_VISION
#import <Foundation/Foundation.h>
#import <ARKit/ARKit.h>
#import <QuartzCore/QuartzCore.h>
#include <os/lock.h>
#include <simd/simd.h>
#include "host_theater.h"

/* a forearm shorter than this is a degenerate reading, not a direction */
#define FOREARM_MINIMUM_METERS 0.01f
/* a barrel this far along the room's up leaves the projection in hand_of
nothing to normalize: about 3 degrees of vertical, where the last up stands
in rather than a direction read out of rounding error */
#define UP_ALONG_BARREL 0.9986f

/* the settings, read once (read_settings); host_spatial.m reads the same ones
for the Sense tracker, and neither owns the other's copy */
static int settings_read;
static int aim_hands;          /* input.aim = "hands" */
static int head_mode;          /* display.stereo = "head" */
static int turn_snap = 1;      /* input.turn == "snap" (the recognizer's own read; host_spatial.m
	reads the same key for the Sense stick and neither owns the other's copy) */
static float deadzone = 18.0f; /* input.hand_turn_deadzone, clamped to 5 to 60: the arm-out turn's
	alone now */
/* input.hand_elevation_deadzone and input.hand_elevation_span, clamped to 2 to 45 and 5 to 60.
The arm's elevation borrowed the turn's deadzone and HANDS_AIM_TURN_RAMP_DEGREES when it was
added, which asked for the arm 45 degrees above its corrected zero for a full deflection; on
device that reads as slow, since nobody's comfortable vertical reach is as long as their
sideways sweep. Two settings rather than two constants so the feel can be converged without a
rebuild, which is how input.hand_turn_deadzone itself was settled */
static float elevation_deadzone = 9.0f;
static float elevation_span = 14.0f;
/* input.hand_aim_gap, clamped to 0 to 1: seconds a hand's aim is held through a tracking
dropout before the guest is told the hand is gone (hands_aim_hold, host_spatial_hands.h).
Defaults to the pinches' own HANDS_HAND_GAP_SECONDS, since the two cover the same blink, and
is a setting rather than a second constant because the blink lengths this has to cover have
not been measured on device yet: 0 asks for the behaviour before the gap existed */
static float aim_gap = 0.15f;
/* input.hand_aim_loss: HANDS_AIM_LOSS_HOLD or HANDS_AIM_LOSS_HEAD (host_spatial_hands.h, which
carries why holding is the default). Read with the same idiom input.aim and input.turn use for
their own values, a comparison against the one non-default spelling with everything else taking
the default, so an unreadable setting holds the aim rather than dropping it */
static int aim_loss = HANDS_AIM_LOSS_HOLD;
static float pitch_offset;     /* input.aim_pitch_offset, clamped to -10 to 45: the same trim
	controller_aim_game.c reads for the barrel ray, read again here so the arm's elevation can
	take it too (host_spatial_hands.c's hands_recognize) */
static int hand_log;           /* debug.hand_log */
static int hand_distance_log;  /* debug.hand_distance_log */
/* debug.hand_distance_log's own rate, in game samples: this file's own assumption is 90 Hz
for a hand's game sample (the same one HANDS_HAND_GAP_SECONDS's and
HANDS_PART_CONFIRM_SAMPLES's comments in host_spatial_hands.h make), and 90 / 9 = 10, so one
line every ninth sample lands close to ten a second without reading a clock this file has
no other reason to read here. Counted rather than timed because the log is for comparing
distances sample to sample while the hand actually moves, not for a fixed wall-clock rate */
#define HAND_DISTANCE_LOG_SAMPLES 9
static int distance_log_counter;

/* one change from a callback, queued for the game thread */
enum hands_event_kind
{
	EVENT_AUTHORIZATION     /* status (ar_authorization_status_t, -1 with an error), text */
};

enum { AUTHORIZATION_NONE, AUTHORIZATION_REQUESTED, AUTHORIZATION_ALLOWED, AUTHORIZATION_DENIED };

@interface HostSpatialHandsEvent : NSObject
@property (nonatomic) int kind;
@property (nonatomic) long status;
@property (nonatomic, copy) NSString *text;
@end

@implementation HostSpatialHandsEvent
@end

static os_unfair_lock lock = OS_UNFAIR_LOCK_INIT;
static NSMutableArray<HostSpatialHandsEvent *> *events;   /* guarded by lock */
/* the queue, the anchors and whether hand tracking exists here, made once */
static int started;
static int supported;
static dispatch_queue_t provider_queue;
static ar_hand_anchor_t anchors[2];       /* the provider fills these: 0 left, 1 right */
static int authorization;
static int wants_authorization;
static int undecided_logged;
/* the session the permission was last looked at in (a new one, the space
opened again, asks again after an undecided answer), and the session the
provider runs in: a provider doesn't outlive its session (host_theater.h) */
static ar_session_t seen_session, ran_session;
static ar_hand_tracking_provider_t provider;
/* the drawing sample's hands, from host_spatial_hands_frame this frame */
static struct halo_spatial_hand draw_hands[2];
static int draw_reason, draw_fresh;
/* each hand's last barrel direction in the room while its forearm was
tracked: an untracked forearm keeps it for that sample */
static simd_float3 last_forward[2];
static int forward_known[2];
/* each hand's last up, for the one case the room cannot give one: a barrel
within UP_ALONG_BARREL of the room's up has nothing left to square against
it, so the last up stands in as the reference and the roll holds near where
it was rather than spinning (aim_up, which squares it against the barrel it
is standing in for) */
static simd_float3 last_up[2];
static int up_known[2];
/* each hand's last wrist transform, which is the aim's position and the grip
outright: kept for the same reason the barrel and the up are, so a sample
inside input.hand_aim_gap has a whole pose to hold and not just a direction
(hand_unreadable). Valid exactly while aim_hold[hand].known is set, since
both are written on the same samples */
static simd_float4x4 last_wrist[2];
/* each hand's aim gap (host_spatial_hands.h's hands_aim_hold): per hand and
independent, so one hand blinking never costs the other its aim */
static struct hands_aim_hold aim_hold[2];
/* what hands_aim_hold answered for each hand on the last game sample, for
debug.hand_log's own line: the reduced joints cannot tell a dropout being
held from one that has run out, since both arrive with tracked 0 */
static int aim_outcome[2];
/* the reduced joints of the last game sample (host_spatial_hands.h): what a
gesture recognizer reads, while the aim itself needs only the wrist and the
forearm */
static struct host_spatial_hand_joints joints[2];
/* the gesture recognizer's own state (pinches, aim turn arm, the prayer
dwell): host_spatial_hands_idle resets it whenever hands were not the game
sample's source, so no pinch or dwell survives into the next one they fill */
static struct hands_state gesture_state;
/* what each hand's aim was built from on the last game sample, and when that
run started: debug.hand_log's own line below (log_aim_condition), reset with
the gesture state because a run cannot span a gap in which hands were not the
source at all. -1 is "no game sample yet", which writes no line, where 0
would read as a run of lost that never happened */
enum { AIM_CONDITION_LOST, AIM_CONDITION_HELD, AIM_CONDITION_HELD_PAST, AIM_CONDITION_CARRIED,
	AIM_CONDITION_MEASURED };
static int aim_condition[2] = { -1, -1 };
static double aim_condition_since[2];
/* 1 while the hands source filled the game sample it was last given */
static int active;
/* the joints the aim and a gesture recognizer need, in struct
host_spatial_hand_joints's order (the reduction below walks them together) */
enum { JOINT_WRIST, JOINT_FOREARM_ARM, JOINT_THUMB_TIP, JOINT_INDEX_TIP, JOINT_MIDDLE_TIP, JOINT_RING_TIP,
	JOINT_LITTLE_TIP,
	/* each tip's chain one, two and three bones back, each family in the same thumb-first
	order, so the reduction below walks a family as one range. These are what a pinch is
	judged by while the thumb is sitting on the fingertip it is pinching and the cameras have
	lost it (host_spatial_hands.h's HANDS_PART_LEVEL_*) */
	JOINT_THUMB_INTERMEDIATE_TIP, JOINT_INDEX_INTERMEDIATE_TIP, JOINT_MIDDLE_INTERMEDIATE_TIP,
	JOINT_RING_INTERMEDIATE_TIP, JOINT_LITTLE_INTERMEDIATE_TIP,
	JOINT_THUMB_INTERMEDIATE_BASE, JOINT_INDEX_INTERMEDIATE_BASE, JOINT_MIDDLE_INTERMEDIATE_BASE,
	JOINT_RING_INTERMEDIATE_BASE, JOINT_LITTLE_INTERMEDIATE_BASE,
	JOINT_THUMB_KNUCKLE, JOINT_INDEX_KNUCKLE, JOINT_MIDDLE_KNUCKLE, JOINT_RING_KNUCKLE,
	JOINT_LITTLE_KNUCKLE,
	JOINT_INDEX_METACARPAL, JOINT_LITTLE_METACARPAL, NUMBER_OF_HAND_JOINTS };
static const ar_hand_skeleton_joint_name_t joint_names[NUMBER_OF_HAND_JOINTS] = {
	ar_hand_skeleton_joint_name_wrist,
	ar_hand_skeleton_joint_name_forearm_arm,
	ar_hand_skeleton_joint_name_thumb_tip,
	ar_hand_skeleton_joint_name_index_finger_tip,
	ar_hand_skeleton_joint_name_middle_finger_tip,
	ar_hand_skeleton_joint_name_ring_finger_tip,
	ar_hand_skeleton_joint_name_little_finger_tip,
	ar_hand_skeleton_joint_name_thumb_intermediate_tip,
	ar_hand_skeleton_joint_name_index_finger_intermediate_tip,
	ar_hand_skeleton_joint_name_middle_finger_intermediate_tip,
	ar_hand_skeleton_joint_name_ring_finger_intermediate_tip,
	ar_hand_skeleton_joint_name_little_finger_intermediate_tip,
	ar_hand_skeleton_joint_name_thumb_intermediate_base,
	ar_hand_skeleton_joint_name_index_finger_intermediate_base,
	ar_hand_skeleton_joint_name_middle_finger_intermediate_base,
	ar_hand_skeleton_joint_name_ring_finger_intermediate_base,
	ar_hand_skeleton_joint_name_little_finger_intermediate_base,
	ar_hand_skeleton_joint_name_thumb_knuckle,
	ar_hand_skeleton_joint_name_index_finger_knuckle,
	ar_hand_skeleton_joint_name_middle_finger_knuckle,
	ar_hand_skeleton_joint_name_ring_finger_knuckle,
	ar_hand_skeleton_joint_name_little_finger_knuckle,
	ar_hand_skeleton_joint_name_index_finger_metacarpal,
	ar_hand_skeleton_joint_name_little_finger_metacarpal
};

static void read_settings(void)
{
	char value[64];

	settings_read = 1;
	host_config_string("input.aim", "controller", value, sizeof(value));
	aim_hands = !strcmp(value, "hands");
	/* input.aim_hand is not read here: which hand the gun is in arrives with every sample
	(halo_spatial_sample's aiming_hand, which host_spatial.m fills from that setting) */
	host_config_string("display.stereo", "off", value, sizeof(value));
	head_mode = !strcmp(value, "head");
	host_config_string("input.turn", "snap", value, sizeof(value));
	turn_snap = !strcmp(value, "snap");
	deadzone = (float)host_config_real("input.hand_turn_deadzone", 18.0);
	if (!(deadzone >= 5.0f && deadzone <= 60.0f))
	{
		float clamped = deadzone > 60.0f ? 60.0f : 5.0f;

		/* the setting only means anything with input.aim = "hands" (its own table
		description), and this file's own read of input.aim is what says so. Nothing reaches
		this file with any other value today, since host_spatial.m's hands_frame and its
		sample branch both test aim_hands before calling in, so the scope is a guard against
		a later caller rather than a case that happens now */
		if (aim_hands)
			host_logf(HOST_LOG_WARN, "spatial: input.hand_turn_deadzone %g is outside 5 to 60; using %g",
				(double)deadzone, (double)clamped);
		deadzone = clamped;
	}
	elevation_deadzone = (float)host_config_real("input.hand_elevation_deadzone", 9.0);
	if (!(elevation_deadzone >= 2.0f && elevation_deadzone <= 45.0f))
	{
		float clamped = elevation_deadzone > 45.0f ? 45.0f : 2.0f;

		/* gated on aim_hands exactly as the turn's deadzone above is, and for the same reason:
		the setting means nothing with any other input.aim, and nothing reaches this file with
		one today */
		if (aim_hands)
			host_logf(HOST_LOG_WARN, "spatial: input.hand_elevation_deadzone %g is outside 2 to 45; using %g",
				(double)elevation_deadzone, (double)clamped);
		elevation_deadzone = clamped;
	}
	elevation_span = (float)host_config_real("input.hand_elevation_span", 14.0);
	if (!(elevation_span >= 5.0f && elevation_span <= 60.0f))
	{
		float clamped = elevation_span > 60.0f ? 60.0f : 5.0f;

		/* the low end matters more here than on either deadzone: the span is a divisor in
		hands_recognize's ramp, so a 0 or a negative from a hand-edited config would invert the
		elevation or hand it an infinity, and clamping to 5 keeps it a positive number of
		degrees whatever arrives */
		if (aim_hands)
			host_logf(HOST_LOG_WARN, "spatial: input.hand_elevation_span %g is outside 5 to 60; using %g",
				(double)elevation_span, (double)clamped);
		elevation_span = clamped;
	}
	aim_gap = (float)host_config_real("input.hand_aim_gap", 0.15);
	if (!(aim_gap >= 0.0f && aim_gap <= 1.0f))
	{
		float clamped = aim_gap > 1.0f ? 1.0f : 0.0f;

		/* gated on aim_hands as the deadzones above are, and clamped the same two ways. The low
		end is 0 and not a floor, because 0 is a meaningful answer here rather than a bad one: it
		asks for the behaviour before the gap existed, where every dropout reaches the guest. A
		NaN lands there too, which is the safe direction, since the other end of this clamp is
		what would hold a stale barrel for a whole second */
		if (aim_hands)
			host_logf(HOST_LOG_WARN, "spatial: input.hand_aim_gap %g is outside 0 to 1; using %g",
				(double)aim_gap, (double)clamped);
		aim_gap = clamped;
	}
	host_config_string("input.hand_aim_loss", "hold", value, sizeof(value));
	aim_loss = !strcmp(value, "head") ? HANDS_AIM_LOSS_HEAD : HANDS_AIM_LOSS_HOLD;
	pitch_offset = (float)host_config_real("input.aim_pitch_offset", 0.0);
	if (!(pitch_offset >= -10.0f && pitch_offset <= 45.0f))
	{
		/* clamped the way controller_aim_game.c's own read of this setting is (its
		PITCH_OFFSET_MINIMUM/MAXIMUM branch), not the two-way idiom above: past either end
		clamps to that end, and a NaN neither a slider nor an environment variable should ever
		produce clamps to 0 rather than to the low end. The host and the guest read the same
		key, and the point of reading it here at all is that the two agree exactly, including
		on the one input that is not really a degree count */
		float clamped = pitch_offset > 45.0f ? 45.0f : pitch_offset < -10.0f ? -10.0f : 0.0f;

		/* gated on aim_hands for the same reason the deadzone's warning above is: this file
		only ever uses the setting while hands are the aim source, and controller_aim_game.c
		has already warned about its own read of it for input.aim = "controller" */
		if (aim_hands)
			host_logf(HOST_LOG_WARN, "spatial: input.aim_pitch_offset %g is outside -10 to 45; using %g",
				(double)pitch_offset, (double)clamped);
		pitch_offset = clamped;
	}
	host_config_string("debug.hand_log", "false", value, sizeof(value));
	hand_log = !strcmp(value, "true");
	host_config_string("debug.hand_distance_log", "false", value, sizeof(value));
	hand_distance_log = !strcmp(value, "true");
}

/* 1 while bare hands are the aim source here. visionOS 26 is where the
immersive path runs at all (host_theater.m's gates), and hand tracking is
older API than that, so this one check stands in for the Sense tracker's SDK
and availability gates */
static int hands_on(void)
{
	if (!settings_read)
		read_settings();
	if (@available(visionOS 26.0, *))
		return aim_hands && head_mode;
	return 0;
}

static NSString *error_text(ar_error_t error)
{
	if (!error)
		return @"no error given";
	NSError *cf = (__bridge_transfer NSError *)ar_error_copy_cf_error(error);
	return [NSString stringWithFormat:@"%@ (code %ld)", cf.localizedDescription, (long)ar_error_get_error_code(error)];
}

/* ---------- the queue of changes: any thread */

static void push(int kind, long status, NSString *text)
{
	HostSpatialHandsEvent *event = [HostSpatialHandsEvent new];

	event.kind = kind;
	event.status = status;
	event.text = text;
	os_unfair_lock_lock(&lock);
	[events addObject:event];
	os_unfair_lock_unlock(&lock);
}

/* ---------- the game thread from here down */

/* the queue, the anchors and whether hand tracking exists here, once. Called
before anything pushes, so the queue is there by the time a handler runs */
static void start(void)
{
	if (started)
		return;
	started = 1;
	supported = ar_hand_tracking_provider_is_supported();
	if (!supported)
	{
		host_logf(HOST_LOG_INFO, "spatial: hand tracking isn't supported here; aiming with the head");
		return;
	}
	events = [NSMutableArray array];
	provider_queue = dispatch_queue_create("org.visr.hands", DISPATCH_QUEUE_SERIAL);
	/* the provider fills anchors the caller owns (hand_tracking.h), so these
	two are made once and refilled, never made per frame */
	anchors[0] = ar_hand_anchor_create();
	anchors[1] = ar_hand_anchor_create();
	/* the first sample asks for the permission: hands have no connect event
	to ask on, as host_spatial.m asks on a controller's */
	wants_authorization = 1;
	host_logf(HOST_LOG_INFO, "spatial: hand tracking aims (input.aim = \"hands\")");
}

static void authorized(HostSpatialHandsEvent *event)
{
	if (event.status == ar_authorization_status_allowed)
	{
		authorization = AUTHORIZATION_ALLOWED;
		host_logf(HOST_LOG_INFO, "spatial: hand tracking allowed");
		return;
	}
	if (event.status == ar_authorization_status_not_determined)
	{
		/* undecided (a dismissed prompt): WAITING, and asked again the next
		time the space opens */
		authorization = AUTHORIZATION_NONE;
		wants_authorization = 0;
		if (!undecided_logged)
		{
			undecided_logged = 1;
			host_logf(HOST_LOG_INFO, "spatial: the hand tracking permission wasn't decided; aiming with the head "
				"until it's asked again (the next time the space opens)");
		}
		return;
	}
	authorization = AUTHORIZATION_DENIED;
	host_logf(HOST_LOG_WARN, "spatial: hand tracking %s; aiming with the head",
		event.status == ar_authorization_status_denied ? "denied" : event.text.UTF8String);
}

/* the provider over both hands, made once per session, and the session run
again with it beside world tracking and whatever else is registered. No data
provider state change handler here: the session takes one and a later set
overrides it (session.h), and the Sense tracker's is the one that logs */
static void track(ar_session_t session)
{
	ar_hand_tracking_configuration_t configuration = ar_hand_tracking_configuration_create();

	provider = ar_hand_tracking_provider_create(configuration);
	/* routine anchor updates are off without a handler (hand_tracking.h);
	the samples fill the anchors themselves. The queue is this file's, never
	the main one, which is what a NULL would ask for */
	ar_hand_tracking_provider_set_update_handler(provider, provider_queue,
		^(ar_hand_anchor_t hand_anchor_left, ar_hand_anchor_t hand_anchor_right) {});
	host_theater_session_set_provider(HOST_THEATER_PROVIDER_HANDS, provider);
	ran_session = session;
	host_logf(HOST_LOG_INFO, "spatial: the hand provider tracks both hands; ARKit's session runs again with it");
}

/* the changes, then the session's own lifecycle: before each sample and each
drawing frame, as the Sense tracker's drain is */
static void drain(void)
{
	NSArray<HostSpatialHandsEvent *> *taken = nil;
	ar_session_t session;

	os_unfair_lock_lock(&lock);
	if (events.count)
	{
		taken = [events copy];
		[events removeAllObjects];
	}
	os_unfair_lock_unlock(&lock);
	for (HostSpatialHandsEvent *event in taken)
		if (event.kind == EVENT_AUTHORIZATION)
			authorized(event);
	if (!supported)
		return;
	/* the space closed, or opened again with a new session: a provider
	doesn't outlive its session, and host_theater.m has already dropped it */
	session = host_theater_session();
	if (ran_session && session != ran_session)
	{
		provider = nil;
		ran_session = nil;
		/* a kept barrel direction or up is a vector in the old session's
		origin, so neither survives into the new one */
		last_forward[0] = last_forward[1] = (simd_float3){ 0.0f, 0.0f, 0.0f };
		forward_known[0] = forward_known[1] = 0;
		last_up[0] = last_up[1] = (simd_float3){ 0.0f, 0.0f, 0.0f };
		up_known[0] = up_known[1] = 0;
		/* and the kept wrist with them, which is a position in the old
		session's origin and the one part of the held pose that would be
		plainly wrong rather than merely stale. The gap's own run goes too, so
		no hold survives into a session that has not seen the hand yet */
		memset(last_wrist, 0, sizeof(last_wrist));
		memset(aim_hold, 0, sizeof(aim_hold));
		aim_outcome[0] = aim_outcome[1] = HANDS_AIM_LOST;
		host_logf(HOST_LOG_INFO, "spatial: ARKit's session ended; the hand provider starts again with the next");
	}
	if (!session)
		return;
	/* a new session (the space opened again) asks again after an undecided
	answer */
	if (session != seen_session)
	{
		seen_session = session;
		if (authorization == AUTHORIZATION_NONE)
			wants_authorization = 1;
	}
	if (wants_authorization && authorization == AUTHORIZATION_NONE)
	{
		authorization = AUTHORIZATION_REQUESTED;
		host_logf(HOST_LOG_INFO, "spatial: asking for the hand tracking permission");
		/* the handler runs on an arbitrary queue: it only pushes. One type
		was asked for, so the result of that type is the answer; the
		enumeration can carry others */
		ar_session_request_authorization(session, ar_hand_tracking_provider_get_required_authorization_type(),
			^(ar_authorization_results_t results, ar_error_t error) {
			__block long status = error ? -1 : ar_authorization_status_not_determined;

			if (results)
				ar_authorization_results_enumerate_results(results, ^bool(ar_authorization_result_t result) {
					if (ar_authorization_result_get_authorization_type(result) == ar_authorization_type_hand_tracking)
						status = ar_authorization_result_get_status(result);
					return true;
				});
			push(EVENT_AUTHORIZATION, status, error ? error_text(error) : nil);
		});
	}
	if (authorization == AUTHORIZATION_ALLOWED && !provider)
		track(session);
}

/* why the hands can or can't aim, in the guest's vocabulary
(halo_spatial.h). READY says the provider runs, not that a hand is in sight:
both hands may still be UNTRACKED, which the guest reads as a lost hand */
static int reason_of(void)
{
	if (!supported)
		return HALO_SPATIAL_NO_HANDS;
	if (authorization == AUTHORIZATION_DENIED)
		return HALO_SPATIAL_HANDS_DENIED;
	if (authorization != AUTHORIZATION_ALLOWED || !provider || !ran_session)
		return HALO_SPATIAL_WAITING;
	return HALO_SPATIAL_READY;
}

/* a room transform into the room's level frame from the head, as
host_spatial.m's location_of. debug.controller_aim_axes has nothing to
calibrate on a transform this file builds itself, so the barrel is always its
-z axis and its up its +y, which is what hand_of builds */
static void location_of(simd_float4x4 transform, const float *head_position, struct halo_spatial_location *location)
{
	float room_from_location[16];

	memcpy(room_from_location, &transform, sizeof(room_from_location));
	host_spatial_location(room_from_location, head_position, HOST_SPATIAL_AXIS_MINUS_Z, HOST_SPATIAL_AXIS_PLUS_Y,
		location);
	/* The head and every hand position must share one origin correction,
	as the Sense tracker's locations do. */
	host_stereo_room_vector(location->forward);
	host_stereo_room_vector(location->left);
	host_stereo_room_vector(location->up);
	host_stereo_room_vector(location->position);
}

/* a joint's room position in the room's level frame from the head, exactly
as a location's position is */
static void position_of(simd_float3 room, const float *head_position, float game[3])
{
	float offset[3] = { room.x - head_position[0], room.y - head_position[1], room.z - head_position[2] };

	host_spatial_room_to_game(offset, game);
	host_stereo_room_vector(game);
}

/* the up that sets the gun's roll, for one barrel direction in the room: the
room's up (the game's +z once host_spatial_room_to_game has had it,
host_spatial_pose.h) projected square to the barrel. Nothing of the hand's
orientation is in it. An open hand rolls freely about the forearm it aims
with, so an up taken off the wrist put that roll straight into the weapon and
the gun rendered on its side; the room cannot do that. Within
UP_ALONG_BARREL of vertical the projection has nothing left to normalize, and
the hand's last up stands in as the reference instead; with none yet, or with
one no better placed than the room's up is, the room's forward does, which is
square to the up a vertical barrel lies along. Whatever the reference, the up
returned is projected square to this barrel and normalized: it is always a
unit vector and never parallel to the barrel */
static simd_float3 aim_up(int hand, simd_float3 forward)
{
	const simd_float3 room_up = { 0.0f, 1.0f, 0.0f }, room_forward = { 0.0f, 0.0f, -1.0f };
	simd_float3 reference = room_up;

	if (fabsf(simd_dot(room_up, forward)) > UP_ALONG_BARREL)
	{
		/* the kept up is a reference like any other and squared against this barrel the same
		way, never handed back as it stands: one kept from a near-level barrel is all but the
		room's up, so a barrel that arrives vertical in a single sample would be given an up
		parallel to itself. host_spatial_pose.c's location_axes crosses the two, which leaves
		a zero left and a zero up, and hand_of stores that back as the kept up, so the roll
		would stay dead until the barrel left the cone. The forearm joint returning after a
		dropout, and a hand leaving view and coming back raised, both reach this in one sample:
		neither clears forward_known or up_known */
		if (up_known[hand] && fabsf(simd_dot(last_up[hand], forward)) <= UP_ALONG_BARREL)
			reference = last_up[hand];
		else
			reference = room_forward;
	}
	return simd_normalize(reference - simd_dot(reference, forward) * forward);
}

/* a sample hand_of could not build an aim from, answered by input.hand_aim_gap
(hands_aim_hold). Inside the gap the last barrel, up and wrist are given out again and the
guest is told the hand is tracked, so a blink of a few samples never reaches it as a loss at
all and the recovery dwell that turns a blink into a visible snap never starts climbing
(host_spatial_hands.h's hands_aim_hold carries the measurement). Past the gap this returns
having written nothing, which leaves out->state at the UNTRACKED hand_of already set, exactly
as before the gap existed.

What is deliberately stale for the length of a hold: the position. The wrist is wherever it
was last seen, so a player whose hand is moving while the anchor blinks aims from a point a
few centimeters behind the hand, and the barrel is the direction it last pointed. Over a
default gap at 90 Hz that is at most thirteen samples of lag on a hand that is in flight, and
the alternative is the head taking the aim outright, which is the thing being fixed. held
stays 1 as it is on the live path: the guest's zoom stabilizer counts an unheld hand as lost
(controller_aim_game.c's hand_sightline), so clearing it here would spend the gap it is
holding */
static void hand_unreadable(int hand, const float *head_position, struct halo_spatial_hand *out, double now)
{
	simd_float4x4 aim = matrix_identity_float4x4;

	/* the gap's decision and the log's record of it in one place, so the three callers below
	cannot take one without the other or take them in the wrong order */
	aim_outcome[hand] = hands_aim_hold(&aim_hold[hand], 0, aim_gap, aim_loss, now);
	if (aim_outcome[hand] != HANDS_AIM_HELD)
		return;
	out->state = HALO_SPATIAL_TRACKED;
	out->held = 1;
	/* the same two columns the live path fills, from the kept pose rather than this sample's */
	aim.columns[1].xyz = last_up[hand];
	aim.columns[2].xyz = -last_forward[hand];
	aim.columns[3] = last_wrist[hand].columns[3];
	location_of(aim, head_position, &out->aim);
	location_of(last_wrist[hand], head_position, &out->grip);
}

/* one hand from the anchor the provider filled: the state, held, the
chirality, the reduced joints and both locations. hand is the anchor's slot
(0 left, 1 right), which is what keeps each hand's last barrel direction.

now is the clock the aim gap is timed on, one reading per sample shared by
both hands (fill). A sample this function cannot build an aim from falls to
hand_unreadable, which holds the last pose for up to input.hand_aim_gap. The
reduced joints are not held with it: they are this sample's own reading or
nothing, so the gesture recognizer keeps deciding its pinches on its own
HANDS_HAND_GAP_SECONDS and the two gaps stay independent */
static void hand_of(int hand, ar_hand_anchor_t anchor, const float *head_position,
	struct halo_spatial_hand *out, struct host_spatial_hand_joints *reduced, double now)
{
	simd_float4x4 origin_from_anchor, room[NUMBER_OF_HAND_JOINTS], wrist, aim = matrix_identity_float4x4;
	int tracked[NUMBER_OF_HAND_JOINTS];
	ar_hand_skeleton_t skeleton;
	simd_float3 forward, up, direction;
	int i;

	memset(out, 0, sizeof(*out));
	memset(reduced, 0, sizeof(*reduced));
	out->state = HALO_SPATIAL_UNTRACKED;
	/* ARKit's hand chirality is right 0 and left 1, the other way around
	from the accessory enum: mapped by name, never by value */
	switch (ar_hand_anchor_get_chirality(anchor))
	{
	case ar_hand_chirality_left: out->chirality = 1; break;
	case ar_hand_chirality_right: out->chirality = 2; break;
	default: out->chirality = 0; break;
	}
	skeleton = ar_hand_anchor_get_hand_skeleton(anchor);
	if (!ar_hand_anchor_is_tracked(anchor) || !skeleton)
	{
		hand_unreadable(hand, head_position, out, now);
		return;
	}
	/* the rendering-corrected transforms, which are what a hand the player
	sees in passthrough is drawn against (hand_tracking.h, skeleton_joint.h) */
	origin_from_anchor = ar_hand_anchor_get_origin_from_anchor_transform(anchor);
	for (i = 0; i < NUMBER_OF_HAND_JOINTS; i++)
	{
		ar_skeleton_joint_t joint = ar_hand_skeleton_get_joint_named(skeleton, joint_names[i]);

		tracked[i] = ar_skeleton_joint_is_tracked(joint);
		room[i] = simd_mul(origin_from_anchor, ar_skeleton_joint_get_anchor_from_joint_transform(joint));
	}
	/* no wrist is no hand, whatever the anchor says: every one of the
	skeleton's transforms is relative to it (skeleton_joint.h) */
	if (!tracked[JOINT_WRIST])
	{
		hand_unreadable(hand, head_position, out, now);
		return;
	}
	wrist = room[JOINT_WRIST];
	/* the reduced joints first, and whatever the aim below can make of them:
	a hand whose forearm hasn't come yet still has fingers worth reading, and
	tracked means what the header says it means, the anchor */
	reduced->tracked = 1;
	reduced->forearm_tracked = tracked[JOINT_FOREARM_ARM];
	position_of(wrist.columns[3].xyz, head_position, reduced->wrist);
	position_of(room[JOINT_FOREARM_ARM].columns[3].xyz, head_position, reduced->forearm_arm);
	for (i = 0; i < 5; i++)
	{
		reduced->tip_tracked[i] = tracked[JOINT_THUMB_TIP + i];
		position_of(room[JOINT_THUMB_TIP + i].columns[3].xyz, head_position, reduced->tip[i]);
		reduced->intermediate_tip_tracked[i] = tracked[JOINT_THUMB_INTERMEDIATE_TIP + i];
		position_of(room[JOINT_THUMB_INTERMEDIATE_TIP + i].columns[3].xyz, head_position,
			reduced->intermediate_tip[i]);
		reduced->intermediate_base_tracked[i] = tracked[JOINT_THUMB_INTERMEDIATE_BASE + i];
		position_of(room[JOINT_THUMB_INTERMEDIATE_BASE + i].columns[3].xyz, head_position,
			reduced->intermediate_base[i]);
		reduced->knuckle_tracked[i] = tracked[JOINT_THUMB_KNUCKLE + i];
		position_of(room[JOINT_THUMB_KNUCKLE + i].columns[3].xyz, head_position, reduced->knuckle[i]);
	}
	for (i = 0; i < 2; i++)
	{
		reduced->metacarpal_tracked[i] = tracked[JOINT_INDEX_METACARPAL + i];
		position_of(room[JOINT_INDEX_METACARPAL + i].columns[3].xyz, head_position, reduced->metacarpal[i]);
	}
	/* the barrel, along the forearm from its arm joint (the elbow end) to
	the wrist. An untracked forearm keeps the last direction for this sample;
	past that the guest's input.aim_loss_grace holds the aim and then the
	head takes it */
	direction = wrist.columns[3].xyz - room[JOINT_FOREARM_ARM].columns[3].xyz;
	if (tracked[JOINT_FOREARM_ARM] && simd_length(direction) > FOREARM_MINIMUM_METERS)
	{
		forward = simd_normalize(direction);
		last_forward[hand] = forward;
		forward_known[hand] = 1;
	}
	else if (forward_known[hand])
		forward = last_forward[hand];
	else
	{
		/* nothing has pointed yet: a wrist on its own can't aim, and there is no pose to hold
		either, so hands_aim_hold answers LOST however wide the gap is (its own !known branch) */
		hand_unreadable(hand, head_position, out, now);
		return;
	}
	out->state = HALO_SPATIAL_TRACKED;
	/* the zoom stabilizer counts an unheld hand as lost
	(controller_aim_game.c's hand_sightline), and a tracked bare hand is as
	held as hand aim gets */
	out->held = 1;
	/* the up, which sets the gun's roll: the room's, square to the barrel
	(aim_up), and kept for the one barrel the room cannot answer for */
	up = aim_up(hand, forward);
	last_up[hand] = up;
	up_known[hand] = 1;
	/* the whole pose kept, and the gap's run ended: all three of the barrel, the up and the
	wrist are stored by the time hands_aim_hold is told this sample was readable, so
	aim_hold[hand].known never promises hand_unreadable a pose that was not written */
	last_wrist[hand] = wrist;
	aim_outcome[hand] = hands_aim_hold(&aim_hold[hand], 1, aim_gap, aim_loss, now);
	/* a room transform in the convention location_of reads: -z the barrel,
	+y the up, the wrist the position. host_spatial_location squares the up
	against the barrel itself (host_spatial_pose.c's location_axes), so only
	the two columns it reads have to be right */
	aim.columns[1].xyz = up;
	aim.columns[2].xyz = -forward;
	aim.columns[3] = wrist.columns[3];
	location_of(aim, head_position, &out->aim);
	/* the grip is the wrist's own transform. The guest reads only aim, but
	host_spatial_hold_poses copies both, so it is filled sanely */
	location_of(wrist, head_position, &out->grip);
}

/* both hands from the two anchors, and the reason for them. predict_to above
0 asks for that instant; a query that can't answer it falls back to the
latest anchors, as the Sense tracker's failed prediction keeps the latest
anchor, so one bad frame time doesn't read as a lost hand.

now is the wall clock the aim gap is timed on, read once by each caller and
passed to both hands. Not predict_to: the gap asks how long ago this hand was
last seen, which is a question about now and not about the instant the
anchors were predicted to. Both samples advance the same gap, as both already
share last_forward and last_up, so either one seeing the hand ends the run */
static int fill(double predict_to, const float *head_position, struct halo_spatial_hand hands[2], int *reason,
	struct host_spatial_hand_joints reduced[2], double now)
{
	int filled = 0;

	*reason = reason_of();
	memset(hands, 0, 2 * sizeof(*hands));
	memset(reduced, 0, 2 * sizeof(*reduced));
	if (*reason != HALO_SPATIAL_READY)
		return 0;
	if (predict_to > 0.0)
		filled = ar_hand_tracking_provider_query_anchors_at_timestamp(provider, predict_to, anchors[0],
			anchors[1]) == ar_hand_anchor_query_status_success;
	if (!filled)
		filled = ar_hand_tracking_provider_get_latest_anchors(provider, anchors[0], anchors[1]);
	if (!filled)
	{
		/* the provider is in the session but isn't running yet */
		*reason = HALO_SPATIAL_WAITING;
		return 0;
	}
	hand_of(0, anchors[0], head_position, &hands[0], &reduced[0], now);
	hand_of(1, anchors[1], head_position, &hands[1], &reduced[1], now);
	return 1;
}

/* one gesture event's name for debug.hand_log (host_spatial_hands.h's enum hands_event's
kind); the pinch's finger is folded in since the four pinches would otherwise log
identically */
static void gesture_name(int kind, int finger, char *buffer, size_t size)
{
	static const char *const finger_names[NUMBER_OF_HANDS_PINCHES] = { "index", "middle", "ring", "little" };

	switch (kind)
	{
	case HANDS_EVENT_PINCH:
	case HANDS_EVENT_PINCH_MISS:
		/* a miss is the same finger ARKit stopped reporting, measured from the position carried
		in the palm's frame instead (host_spatial_hands.h's enum): named apart so a pinch
		reading a carried tip doesn't read as one that opened, and so the gaps' lengths can be
		counted off the log */
		snprintf(buffer, size, "%s %s", kind == HANDS_EVENT_PINCH ? "pinch" : "miss",
			finger >= 0 && finger < NUMBER_OF_HANDS_PINCHES ? finger_names[finger] : "?");
		break;
	case HANDS_EVENT_PART_GROWTH:
		/* "part", one word of its own, so a reader can pull every one of these lines out of a
		session with a single pattern without catching the "(parted)" that path_name puts on
		the end of a pinch line. It is the margin's measurement, not a state change
		(host_spatial_hands.h's enum) */
		snprintf(buffer, size, "part %s",
			finger >= 0 && finger < NUMBER_OF_HANDS_PINCHES ? finger_names[finger] : "?");
		break;
	case HANDS_EVENT_PINCH_HELD:
		/* "held", its own word again, so every release's duration pulls out of a session with
		one pattern: "hands: held index" is every hold of the trigger finger, and the number
		after the side and the "off" is that hold's length in seconds, to the millisecond. The
		tail says which reading released it, which is what separates a tap the fingers ended
		from one that waited for the camera (host_spatial_hands.h's HANDS_EVENT_PINCH_HELD) */
		snprintf(buffer, size, "held %s",
			finger >= 0 && finger < NUMBER_OF_HANDS_PINCHES ? finger_names[finger] : "?");
		break;
	case HANDS_EVENT_AIM_TURN: snprintf(buffer, size, "aim turn"); break;
	/* the aiming stick's other axis, named apart from the turn so a session's elevations can be
	counted off the log on their own (host_spatial_hands.h's HANDS_EVENT_AIM_ELEVATION) */
	case HANDS_EVENT_AIM_ELEVATION: snprintf(buffer, size, "aim elevation"); break;
	case HANDS_EVENT_PRAYER: snprintf(buffer, size, "prayer"); break;
	case HANDS_EVENT_START: snprintf(buffer, size, "start"); break;
	default: snprintf(buffer, size, "?"); break;
	}
}

/* which reading moved a pinch (host_spatial_hands.h's HANDS_PATH_*), for the tail of the
line below: the gaps are what this log is read for, and without this a pinch released
because the fingers were seen to part looks the same in it as one released because the
camera found the fingertip again. Empty for the kinds of event that have no such reading */
static const char *path_name(int path)
{
	static const char *const names[] = { "", " (measured)", " (carried)", " (parted)", " (hand lost)",
		" (blind)" };

	return path >= 0 && path < (int)(sizeof(names) / sizeof(names[0])) ? names[path] : "";
}

/* debug.hand_log: one line per gesture state transition, the value measured against the
threshold it crossed, to three decimals, and which reading decided it. The prayer sign's hand
is always 0 (host_spatial_hands.c's emit calls: it is both hands together, neither one), so it
logs as "both" rather than a side */
static void log_event(const struct hands_event *event)
{
	char gesture[16];

	gesture_name(event->kind, event->finger, gesture, sizeof(gesture));
	host_logf(HOST_LOG_INFO, "hands: %s %s %s %.3f against %.3f%s", gesture,
		event->kind == HANDS_EVENT_PRAYER ? "both" : (event->hand ? "right" : "left"),
		event->on ? "on" : "off", (double)event->value, (double)event->threshold,
		path_name(event->path));
}

/* debug.hand_distance_log's own line, named by where the distance came from
(HANDS_DISTANCE_*, host_spatial_hands.h): "measured", "carried" or "unavailable", in that
order so the enum's own values index straight into it */
static const char *const distance_path_names[] = { "measured", "carried", "unavailable" };

/* debug.hand_distance_log: one line per hand, roughly ten times a second
(HAND_DISTANCE_LOG_SAMPLES above). Four decimals on every distance: three gives 1 mm bins,
too coarse to tell a resting hand from a deliberate pinch apart, which is the whole reason
for this line (host_spatial_hands.h's HANDS_PINCH_CLOSE_RULER_FRACTION comment, which records
the distributions this line measured). Each
of the four pinches carries its distance, which of HANDS_DISTANCE_MEASURED/CARRIED/
UNAVAILABLE it is, and whether that pinch is closed right now, so a session's lines sort
into the resting and the pinching distributions without correlating against any other line.
Each pinch also carries the ruler its own band was scaled off on this sample
(host_spatial_hands.c's ruler_meters), because a distance means nothing without the threshold
it was judged against and every finger's threshold is its own. It is the field that says
whether a finger's window is working: HANDS_RULER_MINIMUM_METERS is 10 mm and a small little
finger's distal segment is about that, so such a finger has every reading of it dropped by
ruler_push and keeps HANDS_RULER_DEFAULT_METERS forever, which puts its pinch at roughly twice
the distance it should be. Exactly 0.0224 in this field is that case, since a median of real
readings does not land on the default.

palm_width and index_distal close the line: two measures of this hand's own size, "unavailable"
rather than a number on a sample ARKit doesn't give them, since the question this line was added
to answer was whether the two thresholds belong in a fraction of one of these rather than in an
absolute distance. They do, and the distal segment is the one they are in now
(host_spatial_hands.c's ruler_meters); both stay on the line, palm width because it is what
says the hand is being seen at the right scale at all. index_distal is the index finger's raw
segment where the ruler fields above are each finger's own smoothed window, so the two are not
the same number and neither replaces the other.

Greppable on "hands: distance ", and every field after the side is in a fixed column, so an
awk invocation can pull any of them by number without parsing the line. The ruler field adds
one column to each finger's group, so a reader counting columns from an earlier session's
lines counts five per finger now and not four */
static void log_distance_sample(int hand, const struct host_spatial_hand_joints *joints)
{
	static const char *const finger_names[NUMBER_OF_HANDS_PINCHES] = { "index", "middle", "ring", "little" };
	struct hands_tip_distances distances;
	/* five fields per finger with "unavailable" in the longest of them, plus the two closing
	measures: 320 keeps the whole line rather than letting the guards below truncate the end of
	it, which is where the hand's own scale sits */
	char line[320];
	size_t used;
	int finger;

	hands_tip_distances(&gesture_state, hand, joints, &distances);
	used = (size_t)snprintf(line, sizeof(line), "hands: distance %s", hand ? "right" : "left");
	for (finger = 0; finger < NUMBER_OF_HANDS_PINCHES && used < sizeof(line); finger++)
		used += (size_t)snprintf(line + used, sizeof(line) - used, " %s %.4f %s %s ruler %.4f",
			finger_names[finger], (double)distances.distance[finger],
			distance_path_names[distances.path[finger]],
			gesture_state.pinch[hand][finger].closed ? "closed" : "open",
			(double)distances.ruler[finger]);
	if (used < sizeof(line))
	{
		if (distances.palm_width_known)
			used += (size_t)snprintf(line + used, sizeof(line) - used, " palm %.4f", (double)distances.palm_width);
		else
			used += (size_t)snprintf(line + used, sizeof(line) - used, " palm unavailable");
	}
	if (used < sizeof(line))
	{
		if (distances.index_distal_known)
			snprintf(line + used, sizeof(line) - used, " index_distal %.4f", (double)distances.index_distal);
		else
			snprintf(line + used, sizeof(line) - used, " index_distal unavailable");
	}
	host_logf(HOST_LOG_INFO, "%s", line);
}

/* debug.hand_log: one line per change in what a hand's aim is being built from, with how long
the run that just ended had lasted. Three conditions, following hand_of's own three outcomes:

	measured  the anchor, the wrist and the forearm are all ARKit's reading, so the barrel is
	          this sample's own and the aim reaches the guest HALO_SPATIAL_TRACKED. A forearm
	          tracked but shorter than FOREARM_MINIMUM_METERS reads as measured here and is
	          carried by hand_of, which is the one place the three names are not exactly its
	          outcomes: a degenerate length is a reading this line has no way to tell from a
	          sound one, and hand_of's own guard is what the number would be read against
	carried   the anchor and the wrist are reported and the forearm is not, so the barrel is
	          last_forward, a direction frozen in the room from whenever the forearm was last
	          seen. Still TRACKED to the guest, which never learns the barrel is stale
	held      no anchor, no skeleton or no wrist, but inside input.hand_aim_gap, so
	          hand_unreadable gives out the last whole pose and the guest is still told
	          TRACKED. Every one of these runs is a dropout the gap absorbed and the guest
	          never saw, which is what the gap is for and what sizing it needs counted
	held_past the same dropout once it has run past the gap, with input.hand_aim_loss = "hold"
	          still giving the pose out. Named apart from held for one reason: it is the split
	          the gap would have made, so a hold-mode session still says which dropouts a
	          head-mode player would have lost the aim to. One dropout therefore writes at most
	          held, then held_past, then its return, however long it lasts
	lost      the dropout past the gap with input.hand_aim_loss = "head", or a dropout with no
	          pose ever to hold in either mode, so the aim is zeroed and HALO_SPATIAL_UNTRACKED
	          and the guest's own grace and ease are all that hold the aim (controller_aim.c's
	          controller_track_update)

Why these four and not the anchor's flag alone: a player reports the gun occasionally
following the head and coming back, and the dropouts that could do it are different faults
with different fixes. A forearm-only drop freezes the barrel where it was and the guest sees
nothing wrong; a wrist or anchor drop reaching the guest is a loss, and there the cost is not
the dropout's length but the recovery dwell it triggers, which a hand dropping repeatedly
walks up to a whole second (port/ios/tests/controller_aim_probe.c's
brief_loss_reaches_the_head measures what that costs). Separating them is the point of the
line: it says which the player is actually hitting, how often, and for how long, and with the
gap in place it says how much of it the gap is already absorbing. A session whose dropouts end
on held says the gap absorbed all of them; dropouts going on to held_past (or, in head mode,
to lost) are the ones it did not, and their held_past runs are how much wider it would have to
be.

Transitions only, so an unbroken run writes nothing however long it lasts, where the per-hand
sample rate would be 90 lines a second. Greppable on "hands: aim " and every field after the
side is in a fixed column. host_logf and the guest's platform_log share stderr (host_main.m's
host_logf writes both it and the runtime log), so these lines and the guest's "controller aim:
hand lost" and "head aims %.3f s after the loss" sort into one stream and can be read against
each other */
static void log_aim_condition(int hand, const struct host_spatial_hand_joints *reduced, int outcome, double now)
{
	static const char *const names[] = { "lost", "held", "held_past", "carried", "measured" };
	/* the outcome first: a held sample and a lost one both arrive with reduced->tracked 0, so
	only hands_aim_hold's answer tells them apart. One word for held_past rather than two, so
	the line's fields stay one per column as the rest of it does */
	int condition = outcome == HANDS_AIM_HELD ?
			(aim_hold[hand].past_gap ? AIM_CONDITION_HELD_PAST : AIM_CONDITION_HELD) :
		outcome != HANDS_AIM_GOOD ? AIM_CONDITION_LOST :
		reduced->forearm_tracked ? AIM_CONDITION_MEASURED : AIM_CONDITION_CARRIED;

	if (aim_condition[hand] == condition)
		return;
	if (aim_condition[hand] >= 0)
		host_logf(HOST_LOG_INFO, "hands: aim %s %s for %.3f s, now %s", hand ? "right" : "left",
			names[aim_condition[hand]], now - aim_condition_since[hand], names[condition]);
	aim_condition[hand] = condition;
	aim_condition_since[hand] = now;
}

void host_spatial_hands_frame(cp_drawable_t drawable, const float head_position[3], double predict_to)
{
	struct host_spatial_hand_joints drawn[2];

	draw_fresh = 0;
	if (!hands_on() || !drawable)
		return;
	start();
	drain();
	/* the drawing sample's joints are thrown away: a gesture reads the game
	sample's, which is the one the buttons would share an instant with */
	fill(predict_to, head_position, draw_hands, &draw_reason, drawn, CACurrentMediaTime());
	draw_fresh = 1;
}

int host_spatial_hands_sample(int kind, struct halo_spatial_sample *sample, const float head_position[3])
{
	struct host_spatial_hand_joints drawn[2];
	int reason = HALO_SPATIAL_WAITING, filled;
	/* one reading of the clock for this sample, shared by the aim gap in fill, the gesture
	recognizer and the aim condition's log, so a gap's run and a pinch's hold are measured
	against the same instant */
	double now = CACurrentMediaTime();

	if (!hands_on())
		return 0;
	start();
	/* the drawing sample host_spatial_hands_frame took this frame, else the
	latest anchors with the head of the last latch */
	if (kind == HALO_SPATIAL_SAMPLE_DRAW && draw_fresh)
	{
		memcpy(sample->hands, draw_hands, sizeof(draw_hands));
		sample->reason = draw_reason;
		return draw_reason == HALO_SPATIAL_READY;
	}
	drain();
	filled = fill(0.0, head_position, sample->hands, &reason,
		kind == HALO_SPATIAL_SAMPLE_GAME ? joints : drawn, now);
	sample->reason = reason;
	if (kind != HALO_SPATIAL_SAMPLE_GAME)
		return filled;
	/* The origin can move between this game sample and the next head latch,
	and the fill above read the anchors in one origin while the head came
	from the other. Keep the previous drawing pose paired with its head until
	that latch rebases, as the Sense tracker's sampleGame: does with its own
	drawing sample; pose loss stays live, and the reference's head values are
	this sample's own, so host_spatial_hold_poses leaves them alone. */
	if (host_stereo_room_recenter_pending())
	{
		struct halo_spatial_sample reference;

		memset(&reference, 0, sizeof(reference));
		reference.head_yaw = sample->head_yaw;
		reference.head_level = sample->head_level;
		memcpy(reference.hands, draw_hands, sizeof(draw_hands));
		host_spatial_hold_poses(sample, &reference);
	}
	active = filled;
	if (!filled)
	{
		/* the provider is in the session but not running yet (fill's own WAITING): no joints
		came in, so the recognizer's pinch and prayer state is reset exactly as
		host_spatial.m's own hands-not-the-source branch resets it (host_spatial_hands_idle)
		-- a gap here is indistinguishable from a lost hand to a pinch that was mid-close */
		host_spatial_hands_idle();
		return filled;
	}
	{
		int g = sample->aiming_hand ? 1 : 0;
		const float *forward = sample->hands[g].aim.forward;
		struct host_spatial_controls controls[2];
		/* 32 is the worst sample the recognizer can fill: eight pinches each ending a carried
		gap, releasing, and reporting both the pair's growth for that release and how long the
		hold it ended had run (host_spatial_hands.c's pinch_update). Below that the recognizer
		drops log lines rather than overrunning, and those two are the lines this size exists
		to keep */
		struct hands_event events[32];
		int count, i;

		/* before the recognizer, so a line about this sample's joints comes out whether or not
		the recognizer has anything to say about them */
		if (hand_log)
		{
			log_aim_condition(0, &joints[0], aim_outcome[0], now);
			log_aim_condition(1, &joints[1], aim_outcome[1], now);
		}

		/* the aim the recognizer turns from is the sample's own, with no fallback: an anchor
		dropout zeroes aim.forward and the recognizer gates the whole aim turn (its event and
		its armed and pulse state, not only stick_x) on joints[g].tracked, so there is nothing
		to hold a last pointing alive for.

		One case does reach the recognizer with joints[g].tracked == 1 and that zeroed forward:
		a wrist tracked whose forearm never has been, where hand_of fills the reduced joints and
		then returns without a barrel. Its yaw reads 0, which in snap mode arms the aim turn, so
		one snap can fire the instant the forearm first appears already past the deadzone. That
		is the turn the arm is pointing at, so it is left alone. */
		count = hands_recognize(&gesture_state, joints, sample->aiming_hand, forward, turn_snap, deadzone,
			elevation_deadzone, elevation_span, pitch_offset, now, controls, events, 32);
		host_spatial_gamepad_map(&controls[0], &controls[1], sample->aiming_hand, &sample->gamepad);
		/* the 2 marks the sample as the hands source's, which xinput_sdl.c
		merges into port 0 like a Sense sample but remaps differently in menus
		(its sense_gamepad_state) */
		sample->gamepad_valid = 2;
		if (hand_log)
			for (i = 0; i < count; i++)
				log_event(&events[i]);
		/* its own route, not these events: the gesture log above is capacity-bounded
		(events[32], hands_recognize's own clamp) for lines worth keeping even on the busiest
		sample, and a periodic raw-distance line has nothing to do with that budget and no
		business crowding it. host_logf has no such cap, so this costs nothing events does not
		already cost and nothing when debug.hand_distance_log is off */
		if (hand_distance_log && ++distance_log_counter >= HAND_DISTANCE_LOG_SAMPLES)
		{
			distance_log_counter = 0;
			log_distance_sample(0, &joints[0]);
			log_distance_sample(1, &joints[1]);
		}
	}
	return filled;
}

int host_spatial_hands_active(void)
{
	return active;
}

void host_spatial_hands_idle(void)
{
	memset(&gesture_state, 0, sizeof(gesture_state));
	/* and the aim condition's run, which is not memset with the rest: 0 is the lost condition
	and would date the next line from a run that never happened, where -1 writes no line at all
	until the next game sample has said what the hands are doing (log_aim_condition) */
	aim_condition[0] = aim_condition[1] = -1;
	aim_condition_since[0] = aim_condition_since[1] = 0.0;
	/* and the aim gap, for the reason its own struct comment gives: hands were not the source
	for some stretch here, and a run timed across that stretch would hold an aim on the strength
	of a sample from before it. The next readable sample starts a fresh hold */
	memset(aim_hold, 0, sizeof(aim_hold));
	aim_outcome[0] = aim_outcome[1] = HANDS_AIM_LOST;
}
#endif
