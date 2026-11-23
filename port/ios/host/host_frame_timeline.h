/*
HOST_FRAME_TIMELINE.H

The open frame timeline row (port/runtime/frame_timeline.h) on the thread that
presents, the late-join ring and debug.latency_log. The row opens at the game's
first controller read of a frame, the backend writes what only it knows before
its present returns, and present_wrapped seals it (the timeline spec's decision
note: on the game thread, after the present barrier). A drawable's presented
handler patches the shown time by frame id from any thread.
*/

#ifndef __HALO_HOST_FRAME_TIMELINE_H
#define __HALO_HOST_FRAME_TIMELINE_H

#include <stdint.h>

/* the game read the controllers (after halo_frame_start returned): the first call of a frame stamps it */
void host_frame_timeline_controller_read(void);
/* the open row's frame id, for a completion handler to patch the row later */
uint64_t host_frame_timeline_frame_id(void);
/* the backend's part of the open row, before its present returns: the frame's display wait and, with the encode
thread, the encoder's tail (seconds; a negative value leaves the field as it is) */
void host_frame_timeline_waits(double display_wait, double encoder_tail);
/* with the encode thread, after the present barrier: the game thread's wait there (seconds); the row's encoder tail
is the part that was not the display wait */
void host_frame_timeline_barrier(double waited);
/* the pacer's late-start sleep before the open row's controller read (seconds; on the game thread) */
void host_frame_timeline_late_start(double slept);
/* 1 with debug.latency_log: the backend asks the drawable for its presented time only then */
int host_frame_timeline_logging(void);
/* the display showed frame_id at shown (CACurrentMediaTime's clock; 0 never shown): any thread */
void host_frame_timeline_shown(uint64_t frame_id, double shown);
/* present_wrapped, as the backend's present returns: the frame's last CPU act (presentDrawable and the commit) is
done, its submit time; stores the row and, with debug.latency_log, logs the window's line */
void host_frame_timeline_seal(void);

#endif
