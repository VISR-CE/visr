#pragma once

/* Read once before a game thread starts; config changes require a restart. */
int host_game_thread_enabled(void);

/* Main-thread only, whether called by the poll or a run-loop observer. */
void host_event_pump(void);
