/* display.frame_deadline's deadline for a frame's work (gpu_metal.m), in plain C so that
   port/ios/tests/frame_deadline_probe.c checks it natively. */
#ifndef HALO_FRAME_DEADLINE_H
#define HALO_FRAME_DEADLINE_H

/* the seconds a frame's work has: the period of the frame rate in force (target: display.frame_rate's cap or a scene
   class's rate, host_frame_rate_target; 0 for none), else the display's refresh period; 1/60 s when neither is
   plausible (20 to 240 frames a second) */
static inline double frame_deadline_period(double target, double refresh_period)
{
	double period = target >= 20.0 && target <= 240.0 ? 1.0 / target : refresh_period;

	return period >= 1.0 / 240.0 && period <= 1.0 / 20.0 ? period : 1.0 / 60.0;
}

/* The drawing thread's membership of the "VISR frame" workgroup (gpu_metal.m). UIKit starts its own intervals on the
   main thread, which is the drawing thread, and raises an Objective-C exception from _beginInterval when the thread
   belongs to a workgroup that is not UIKit's (a drag of the benchmark menu's scroll view, a UIKit update cycle),
   so the thread leaves the workgroup before UIKit runs its own loop and joins again at the next frame. */
enum { FRAME_DEADLINE_OFF = -1, FRAME_DEADLINE_NEW = 0, FRAME_DEADLINE_JOINED = 1, FRAME_DEADLINE_LEFT = 2 };

/* the state once the thread that called leaves: only the joined thread can leave, and only while it is joined */
static inline int frame_deadline_state_after_leave(int state, int on_joined_thread)
{
	return state == FRAME_DEADLINE_JOINED && on_joined_thread ? FRAME_DEADLINE_LEFT : state;
}

/* whether a leave has anything to do: only the thread that is joined leaves, and once, so a second leave with no frame
   (and no rejoin) in between does not call os_workgroup_leave for a thread that is no longer a member */
static inline int frame_deadline_leave_wanted(int state, int on_joined_thread)
{
	return state == FRAME_DEADLINE_JOINED && on_joined_thread;
}

/* a frame that is shown joins again after a leave; an offscreen benchmark frame has no deadline and stays out */
static inline int frame_deadline_rejoin_wanted(int state, int offscreen)
{
	return state == FRAME_DEADLINE_LEFT && !offscreen;
}

#endif
