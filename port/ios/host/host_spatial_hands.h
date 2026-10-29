/* Bare hands as the aim source (input.aim = "hands"): host_spatial_hands.m
fills the same structs the Sense controllers fill (halo_spatial.h), so the
guest's aim path (controller_aim_game.c) sees one kind of hand.

Split on purpose: the reduced joints below are plain C, so a host-side test
can check what reads them (as host_spatial_gamepad.h is), and only the two
samples and the source flag need CompositorServices and Objective-C. */
#ifndef HOST_SPATIAL_HANDS_H
#define HOST_SPATIAL_HANDS_H

#include "halo_spatial.h"

/* one hand's joints, reduced for the recognizer: positions in the room's
level frame turned into the game's axes, meters from the head, exactly as
halo_spatial_location's position is (host_spatial_pose.h) */
struct host_spatial_hand_joints
{
	int tracked;                 /* the anchor */
	int forearm_tracked;
	float wrist[3], forearm_arm[3];
	int tip_tracked[5];          /* thumb, index, middle, ring, little */
	float tip[5][3];
	int metacarpal_tracked[2];   /* index, little */
	float metacarpal[2][3];
};

#ifdef __OBJC__
#import <CompositorServices/CompositorServices.h>

/* the drawing sample's hands, queried at predict_to (the drawable's
trackable-anchor time, the instant the Sense tracker predicts to) and held
for the drawing sample that follows in this frame; a NULL drawable only
drops the held one, as host_spatial_frame's does */
void host_spatial_hands_frame(cp_drawable_t drawable, const float head_position[3], double predict_to);
/* fills the sample's hands, reason and (game sample) the gamepad from the
latest hand anchors; returns 1 when it filled, 0 when hands are not the
source this sample (unsupported, off, no session) */
int host_spatial_hands_sample(int kind, struct halo_spatial_sample *sample, const float head_position[3]);
/* 1 while the hands source filled the game sample it was last given. The
Sense tracker wins while it is READY and then this is never asked, so
host_spatial.m's precedence, not this flag, decides who owns a sample */
int host_spatial_hands_active(void);
#endif

#endif
