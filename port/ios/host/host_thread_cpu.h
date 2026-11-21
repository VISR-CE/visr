/*
HOST_THREAD_CPU.H

debug.thread_cpu_log: every 300 presented frames, one log line with the
frame's wall time and, per frame, the CPU time of the whole app, of the main
thread, of the presenting thread and of each other thread that used any (by
name), and the app's voluntary and involuntary context switches. It shows how
busy each core is, and so how much room a chip with two cores has for another
thread.
*/

#ifndef __HALO_HOST_THREAD_CPU_H
#define __HALO_HOST_THREAD_CPU_H

/* reads the setting; call once, before the first frame */
void host_thread_cpu_install(void);
/* at each present, on the presenting thread */
void host_thread_cpu_frame(void);

#endif
