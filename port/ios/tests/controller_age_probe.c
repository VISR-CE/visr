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
    host_controller_age_sample();
    assert(logs == 0);
    host_controller_refresh();
    assert(updates == 1 && host_controller_age_ns() == 0);
    /* a read 8.3 ms after the main pump sees 8.3 ms old state ... */
    clock_ns += 8333000;
    assert(host_controller_age_ns() == 8333000);
    /* ... and the game thread's poll just before the read makes it fresh */
    host_controller_refresh();
    clock_ns += 50000;
    assert(updates == 2 && host_controller_age_ns() == 50000);
    host_controller_refreshed();
    assert(host_controller_age_ns() == 0);
    /* the window logs after 5 s with the distribution of its reads */
    for (int read = 0; read < 600; read++) {
        host_controller_refresh();
        clock_ns += 1000000;
        host_controller_age_sample();
    }
    clock_ns += 5000000000ull;
    host_controller_age_sample();
    assert(logs == 1);
    assert(strstr(last_log, "controller age: reads "));
    assert(strstr(last_log, "p99 1.00") || strstr(last_log, "p99 1.25"));
    puts("PASS: controller age is the time since the last update; a poll resets it");
    return 0;
}
