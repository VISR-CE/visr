#include "host_config.h"
#include "host_game_thread.h"
#include "host_controller_age.h"
#include <SDL3/SDL_events.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *configured;
static int config_reads, pumps, polls, peeps, window_refreshes;
static int on_main = 1, peep_result = 1;
static int controller_refreshes, controller_pump_notes;

/* the game thread brings the controllers up to date at its own poll, the main pump only notes it */
void host_controller_refresh(void) {}
void host_controller_refresh_bounded(void) { controller_refreshes++; }
static int read_samples;
void host_controller_sample(void) { read_samples++; }
void host_controller_refreshed(void) { controller_pump_notes++; }
static int read_refreshes;
void host_controller_refresh_for_read(void) { read_refreshes++; }

void host_config_string(const char *name, const char *fallback, char *out, size_t size) {
    assert(strcmp(name, "debug.game_thread") == 0);
    assert(strcmp(fallback, "true") == 0);
    config_reads++;
    snprintf(out, size, "%s", configured ? configured : fallback);
}

int host_on_main_thread(void) {
    return on_main;
}

void host_sdl_refresh_window_cache(void) {
    assert(on_main && pumps == window_refreshes + 1);
    window_refreshes++;
}

void SDL_PumpEvents(void) {
    assert(on_main);
    pumps++;
}

bool SDL_PollEvent(SDL_Event *event) {
    polls++;
    event->type = SDL_EVENT_QUIT;
    return polls == 1;
}

int SDL_PeepEvents(SDL_Event *event, int count, SDL_EventAction action, Uint32 first, Uint32 last) {
    assert(count == 1);
    assert(action == SDL_GETEVENT);
    assert(first == SDL_EVENT_FIRST && last == SDL_EVENT_LAST);
    peeps++;
    event->type = SDL_EVENT_QUIT;
    return peep_result;
}

/* host_sdl.c's no-wait zone around controller reads (host_main_thread.h), stubbed with it */
static int pad_reads_open;
static void pad_read_begin(void) { pad_reads_open++; }
static void pad_read_end(void) { assert(pad_reads_open > 0); pad_reads_open--; }

/* tools/ios_test.py extracts the production helper without the rest of host_sdl.c. */
#include "game_thread_next_event.inc"

int main(int argc, char **argv) {
    assert(argc == 1 || argc == 2);
    configured = argc == 2 ? argv[1] : NULL;
    int expected = strcmp(configured ? configured : "true", "true") == 0;
    assert(host_game_thread_enabled() == expected);
    assert(config_reads == 1);
    configured = expected ? "false" : "true";
    assert(host_game_thread_enabled() == expected);
    assert(host_game_thread_enabled() == expected);
    assert(config_reads == 1);
    assert(pumps == 0);
    host_event_pump();
    assert(pumps == 1);
    SDL_Event event;
    assert(next_event(&event));
    assert(event.type == SDL_EVENT_QUIT);
    peep_result = 0;
    assert(!next_event(&event));
    peep_result = -1;
    assert(!next_event(&event));
    assert(pumps == (expected ? 4 : 1));
    on_main = 0;
    peep_result = 1;
    assert(next_event(&event) == expected);
    assert(pumps == (expected ? 4 : 1));
    assert(polls == (expected ? 0 : 4));
    assert(peeps == (expected ? 4 : 0));
    /* class: a read on the game thread never relies on the main timer's pump */
    assert(controller_refreshes == (expected ? 1 : 0));
    /* a controller read the poll's gate left behind: the game thread's own read refreshes, main's and the
       thread-off build's do not */
    host_sdl_update_joysticks();
    assert(read_refreshes == (expected ? 1 : 0) && read_samples == (expected ? 0 : 1));
    on_main = 1;
    host_sdl_update_joysticks();
    assert(read_refreshes == (expected ? 1 : 0) && read_samples == (expected ? 1 : 2));
    on_main = 0;
    /* both reads ran inside the no-wait zone, and left it */
    assert(pad_reads_open == 0);
    assert(controller_pump_notes == pumps + (expected ? 0 : polls));
    assert(config_reads == 1);
    assert(window_refreshes == pumps);
    puts("PASS: cached game-thread flag; legacy poll or main pump and peep drain");
    return 0;
}
