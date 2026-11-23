/*
HOST_MAIN_SYNC_LOG.H

debug.main_sync_log: every 300 presented frames, one log line with how long
the game thread was blocked on the main thread a frame (host_main_sync and
host_main_sync_f made off main) and how many round trips it made, as median,
95th percentile and worst frame, plus the longest single wait. It sizes how
much of a frame goes to waiting on main, which a frame should not do.
Only waits made through host_main_sync and host_main_sync_f are counted: a
raw dispatch_sync to the main queue or performSelectorOnMainThread with
waitUntilDone does not show up here. With debug.encode_thread = "thread" the
renderer's waits on main happen on the encode thread and are not counted
either, though the game thread still pays for them at the present.
*/

#ifndef __HALO_HOST_MAIN_SYNC_LOG_H
#define __HALO_HOST_MAIN_SYNC_LOG_H

/* reads the setting; call once, before the first frame */
void host_main_sync_log_install(void);
/* at each present, on the game thread */
void host_main_sync_log_frame(void);

#endif
