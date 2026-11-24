/* Whether a map load's change of workload context switches the Metal encode thread (host_gpu_dispatch.c's
   encode_queue_rewrap), in plain C so that port/ios/tests/encode_switch_probe.c checks it natively. */
#ifndef HALO_ENCODE_SWITCH_H
#define HALO_ENCODE_SWITCH_H

/* metal: the Metal backend draws; have_unwrapped: the backend before the queue is known; debug_set: debug.encode_thread
   chose the mode and a context does not change it; scene_thread: display.encode_thread as the game resolved it for the
   context (-1 not told, 0 off, 1 thread); queue_possible: the stream ring has the set the queue needs and the build can
   run it (not visionOS or Mac Catalyst); on_main_thread: the renderer runs on the main thread, where the present barrier
   would hold the main thread; thread_on: the encode thread is in force now. Returns 1 to wrap the backend again. */
static inline int encode_switch_wanted(int metal, int have_unwrapped, int debug_set, int scene_thread, int queue_possible,
	int on_main_thread, int thread_on)
{
	if (!metal || !have_unwrapped || debug_set || scene_thread < 0)
		return 0;
	if (scene_thread == 1 && (!queue_possible || on_main_thread))
		return 0;
	return (scene_thread == 1) != (thread_on != 0);
}

#endif
