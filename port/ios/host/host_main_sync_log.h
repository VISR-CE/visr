/*
HOST_MAIN_SYNC_LOG.H

debug.main_sync_log: every 300 presented frames, one log line with how long
the presenting thread was blocked on the main thread a frame (host_main_sync
and host_main_sync_f made off main) and how many round trips it made, as
median, 95th percentile and worst frame, plus the longest single wait. It
sizes the input latency audit's fix 6 (no waits on main inside a frame).
*/

#ifndef __HALO_HOST_MAIN_SYNC_LOG_H
#define __HALO_HOST_MAIN_SYNC_LOG_H

/* reads the setting; call once, before the first frame */
void host_main_sync_log_install(void);
/* at each present, on the presenting thread */
void host_main_sync_log_frame(void);

#endif
