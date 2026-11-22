/* Player one's merged pad (host_touch.m, host_tv.m): a virtual pad (the touch controls, the Siri
   Remote, visionOS's keyboard) owns the first port and merges in the first hardware controller;
   further controllers take the next ports. Enumerations and pad reads run on any thread: main's
   event pump, the game thread and the guest's own threads all make them. So the primary
   controller is an atomic, written whole by each enumeration and loaded once per use; a read
   never sees an enumeration half done. The virtual pad's id is set once on main before any other
   thread starts. */
#ifndef HALO_HOST_GAMEPAD_MERGE_H
#define HALO_HOST_GAMEPAD_MERGE_H
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdint.h>

struct host_gamepad_merge {
    SDL_JoystickID virtual_id;
    _Atomic SDL_JoystickID primary;
};

/* The ports, virtual pad first, into out; the count. skip (or NULL) leaves a controller out of
   the ports altogether. *primary_out and *previous (each may be NULL) get the primary this
   enumeration found and the one before it. */
static inline int host_gamepad_merge_enumerate(struct host_gamepad_merge *merge, uint32_t *out, int capacity,
    int (*skip)(uint32_t id), SDL_JoystickID *primary_out, SDL_JoystickID *previous) {
    int count = 0, used = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    SDL_JoystickID primary = 0;
    if (capacity > 0 && merge->virtual_id) out[used++] = merge->virtual_id;
    for (int i = 0; ids && i < count; i++) {
        if (ids[i] == merge->virtual_id || (skip && skip((uint32_t)ids[i]))) continue;
        if (!primary) {
            primary = ids[i];
            if (!SDL_GetGamepadFromID(ids[i])) SDL_OpenGamepad(ids[i]);
        } else if (used < capacity) out[used++] = ids[i];
    }
    SDL_JoystickID before = atomic_exchange(&merge->primary, primary);
    if (primary_out) *primary_out = primary;
    if (previous) *previous = before;
    SDL_free(ids);
    return used;
}

static inline int host_gamepad_merge_is_virtual(struct host_gamepad_merge *merge, SDL_Gamepad *pad) {
    return merge->virtual_id && SDL_GetGamepadID(pad) == merge->virtual_id;
}

/* The primary controller as of the last enumeration, or NULL */
static inline SDL_Gamepad *host_gamepad_merge_primary(struct host_gamepad_merge *merge) {
    SDL_JoystickID primary = atomic_load(&merge->primary);
    return primary ? SDL_GetGamepadFromID(primary) : NULL;
}
#endif
