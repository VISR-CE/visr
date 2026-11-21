#pragma once
#include <stdint.h>

/* Brings SDL's controller state up to date. Safe from any thread: the game
   thread calls it at its own input poll, so the state it reads is as old as
   that poll and not as old as the main run loop's last pump. */
void host_controller_refresh(void);

/* The main-thread pump (SDL_PumpEvents) just updated the controllers. */
void host_controller_refreshed(void);

/* Nanoseconds since the controllers were last brought up to date, or
   UINT64_MAX before the first update. */
uint64_t host_controller_age_ns(void);

/* One game read of controller state: adds its age to the window and, with
   debug.controller_age_log, logs count, mean, p50, p99 and max every 5 s. */
void host_controller_age_sample(void);
