#include "host_game_thread.h"
#include "ios_host.h"
#include "host_config.h"
#include "host_main_thread.h"
#include "host_controller_age.h"
#include <SDL3/SDL_events.h>
#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

int host_game_thread_enabled(void) {
#if TARGET_OS_VISION || TARGET_OS_MACCATALYST
    /* host_main only starts the game thread on iOS, tvOS and the native Mac. */
    return 0;
#else
    /* (-1 until read; controller reads ask it on any thread, host_sdl_update_joysticks) */
    static atomic_int cached = -1;
    int enabled = atomic_load(&cached);
    if (enabled < 0) {
        char value[16];
        host_config_string("debug.game_thread", "true", value, sizeof(value));
        enabled = strcmp(value, "true") == 0;
        atomic_store(&cached, enabled);
    }
    return enabled;
#endif
}

void host_event_pump(void) {
    assert(host_on_main_thread());
    SDL_PumpEvents();
    host_controller_refreshed();
    host_sdl_refresh_window_cache();
#if TARGET_OS_TV
    /* tvOS's text dialog gate, decided here on main so the game thread's pad reads only load it (host_tv.m) */
    host_tv_text_prompt_tick();
#endif
}
