#include "host_game_thread.h"
#include "host_config.h"
#include "host_main_thread.h"
#include <SDL3/SDL_events.h>
#include <assert.h>
#include <string.h>

int host_game_thread_enabled(void) {
    static int enabled = -1;
    if (enabled < 0) {
        char value[16];
        host_config_string("debug.game_thread", "false", value, sizeof(value));
        enabled = strcmp(value, "true") == 0;
    }
    return enabled;
}

void host_event_pump(void) {
    assert(host_on_main_thread());
    SDL_PumpEvents();
}
