#pragma once
#include <stdint.h>

/* Brings SDL's controller state up to date. Safe from any thread: the game
   thread calls it at its own input poll, so the state it reads is as old as
   that poll and not as old as the main run loop's last pump. SDL event
   watchers and the event filter run on the calling thread, so none may touch
   main-only state for joystick or gamepad events. */
void host_controller_refresh(void);

/* A game read of controller state, on the game thread: logs the age the read would have seen (with
   debug.controller_age_log), then refreshes unless the state is under HOST_CONTROLLER_READ_MAX_AGE_NS
   old, so the reads of one poll (four ports, the late look) share one update. */
#define HOST_CONTROLLER_READ_MAX_AGE_NS 250000ull
void host_controller_refresh_for_read(void);

/* The same sample without the refresh, for a read on the main thread or with the game thread off
   (so Vision Pro and the thread-off builds report an age too). */
void host_controller_sample(void);

/* Refreshes unless the state is under the read bound old: the poll's update after each event of one pump
   (a pump delivering N events is one update, not N + 1). */
void host_controller_refresh_bounded(void);

/* The main-thread pump (SDL_PumpEvents) just updated the controllers. */
void host_controller_refreshed(void);

/* Nanoseconds since the controllers were last brought up to date, or
   UINT64_MAX before the first update. */
uint64_t host_controller_age_ns(void);
