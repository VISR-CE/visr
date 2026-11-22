/* host_main_sync from another thread is safe only because the main thread never
   blocks waiting on the game thread. Main posts to the game thread only through
   atomics, queues or semaphores the game thread drains. The same rule applies to
   host_main_sync_f. */
#pragma once
#include <dispatch/dispatch.h>

int host_on_main_thread(void);
#ifdef __BLOCKS__
void host_main_async(dispatch_block_t block);
void host_main_sync(dispatch_block_t block);
#endif
void host_main_async_f(void *context, void (*fn)(void *));
void host_main_sync_f(void *context, void (*fn)(void *));

/* Code that must never wait on main, such as a controller read (host_sdl.c's gamepad entry points),
   runs between host_main_wait_forbid and host_main_wait_allow (they nest). A host_main_sync or
   host_main_sync_f made in between from another thread is still made, and counted:
   host_main_forbidden_waits gives the count, for a log line and the probes. It sees only waits
   made through these two: not a dispatch_sync of its own, nor one inside SDL, nor a lock main
   holds (a pad read takes SDL's joystick lock, which main's SDL_PumpEvents also takes). */
void host_main_wait_forbid(void);
void host_main_wait_allow(void);
unsigned long host_main_forbidden_waits(void);
