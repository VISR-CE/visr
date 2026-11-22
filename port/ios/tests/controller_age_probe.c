/* host_controller_age.c with SDL's clock and joystick update replaced: the age
   a game read sees is the time since the last update from any thread, and a
   game-thread poll resets it. */
#include "host_controller_age.h"
#include "host.h"
#include "host_config.h"
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_timer.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static Uint64 clock_ns;
static int updates, logs;
static char last_log[160];

Uint64 SDL_GetTicksNS(void) { return clock_ns; }
void SDL_UpdateJoysticks(void) { updates++; }
void host_config_string(const char *name, const char *fallback, char *out, size_t size) {
    assert(strcmp(name, "debug.controller_age_log") == 0);
    snprintf(out, size, "%s", "true");
    (void)fallback;
}
void host_logf(int priority, const char *format, ...) {
    (void)priority;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(last_log, sizeof(last_log), format, arguments);
    va_end(arguments);
    logs++;
}

int main(void) {
    clock_ns = 1000000;
    assert(host_controller_age_ns() == UINT64_MAX);
    /* a first read has no state to be old: no sample, and it brings the controllers up to date */
    host_controller_refresh_for_read();
    assert(logs == 0 && updates == 1 && host_controller_age_ns() == 0);
    /* a read 8.3 ms after the last update (the main timer's) is refreshed first: it never sees that age */
    clock_ns += 8333000;
    assert(host_controller_age_ns() == 8333000);
    host_controller_refresh_for_read();
    assert(updates == 2 && host_controller_age_ns() == 0);
    /* the class: a game read never sees state older than HOST_CONTROLLER_READ_MAX_AGE_NS, whatever its
       interval from the previous update; reads of one poll (four ports, the late look) share one update */
    for (int gap = 0; gap < 40; gap++) {
        clock_ns += (Uint64)gap * 400000;
        host_controller_refresh_for_read();
        assert(host_controller_age_ns() < HOST_CONTROLLER_READ_MAX_AGE_NS);
    }
    int before = updates;
    for (int read = 0; read < 10; read++) {
        clock_ns += 10000; /* ten reads over 100 us: one poll's */
        host_controller_refresh_for_read();
    }
    assert(updates - before <= 1);
    clock_ns += 1000000;
    host_controller_refreshed();
    assert(host_controller_age_ns() == 0);
    /* the log reports the age the reads would have seen before their refresh, in 5 s windows */
    logs = 0;
    for (int read = 0; read < 600; read++) {
        clock_ns += 1000000;
        host_controller_refresh_for_read();
        host_controller_refresh_for_read(); /* the second read of the poll: nothing to refresh */
    }
    clock_ns += 5000000000ull;
    host_controller_refresh_for_read();
    assert(logs >= 1);
    assert(strstr(last_log, "controller age: reads "));
    puts("PASS: a game read never sees controller state older than the read bound; the age log samples before the refresh");
    return 0;
}
