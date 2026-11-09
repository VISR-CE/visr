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
