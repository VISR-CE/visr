/* The Apple TV's merged pad (host_tv.m): the Siri Remote's latches, the text dialog's gate and the pad reads,
   extracted from the production file by tools/ios_test.py (tv_gamepad_gate.inc), with SDL's gamepad calls and
   the dialog's UIKit side stubbed here. Checks what the gate does (reads give nothing while the dialog is up or a
   button that closed it is still held, remote presses are dropped then, only main decides it), then runs pad reads
   and an enumeration on other threads while main ticks, presses and changes the dialog, for the thread sanitizer.
   The class: state a pad read shares with another thread is atomic, and a pad read never waits on main. */
/* (the macOS compiler's TARGET_OS_VISION is 0: the extracted code takes its tvOS branches) */
#include <SDL3/SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_gamepad_merge.h"
#include "thread_passes.h"

static _Thread_local int on_main;
int host_on_main_thread(void) { return on_main; }
int host_sdl_controller_aim_owns(uint32_t id) { (void)id; return 0; }

/* the dialog's UIKit side: main's alone, as in host_tv.m */
static bool dialog_up;
static int hint_changes, deadline_holds;
static bool text_prompt_up(void) { assert(on_main); return dialog_up; }
static void text_prompt_hint(bool on) { assert(on_main); (void)on; hint_changes++; }
static void gpu_metal_frame_deadline_hold(int hold) { assert(on_main); deadline_holds += hold ? 1 : -1; }
static atomic_bool hardware_down;
static bool hardware_held(void) { assert(on_main); return atomic_load(&hardware_down); }
void host_ios_touch_reset(void);
static bool text_input_blocked_cached(void);

/* SDL's gamepads: id 1 the remote's virtual pad, id 2 a hardware controller */
struct SDL_Gamepad {
    SDL_JoystickID id;
    _Atomic Sint16 axes[SDL_GAMEPAD_AXIS_COUNT];
    atomic_bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
};
static SDL_Gamepad remote = {.id = 1}, hardware = {.id = 2};
SDL_JoystickID *SDL_GetGamepads(int *count) {
    SDL_JoystickID *ids = malloc(3 * sizeof(*ids));
    assert(ids);
    ids[0] = 1; ids[1] = 2; ids[2] = 0;
    *count = 2;
    return ids;
}
void SDL_free(void *memory) { free(memory); }
SDL_Gamepad *SDL_GetGamepadFromID(SDL_JoystickID id) { return id == 1 ? &remote : id == 2 ? &hardware : NULL; }
SDL_Gamepad *SDL_OpenGamepad(SDL_JoystickID id) { return SDL_GetGamepadFromID(id); }
SDL_JoystickID SDL_GetGamepadID(SDL_Gamepad *pad) { return pad->id; }
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *pad, SDL_GamepadAxis axis) { return atomic_load(&pad->axes[axis]); }
bool SDL_GetGamepadButton(SDL_Gamepad *pad, SDL_GamepadButton button) { return atomic_load(&pad->buttons[button]); }
SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad *pad) { (void)pad; return SDL_GAMEPAD_TYPE_PS5; }

/* tools/ios_test.py extracts these from host_tv.m */
#include "tv_gamepad_gate.inc"

static bool key(SDL_Scancode code, bool down) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = code;
    event.key.down = down;
    return remote_event(NULL, &event);
}

static void *tick_off_main(void *context) {
    (void)context;
    host_tv_text_prompt_tick();
    return NULL;
}

static atomic_int stop;
/* the other threads' passes (thread_passes.h): two readers, then the enumerator */
enum { READERS = 2, THREADS = 3 };
static struct thread_passes passes;
static void *reader(void *context) {
    atomic_long *count = context;
    while (!atomic_load(&stop)) {
        for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; button++)
            (void)host_ios_gamepad_button(&remote, button);
        for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; axis++)
            (void)host_ios_gamepad_axis(&remote, axis);
        (void)host_ios_gamepad_type(&remote);
        (void)host_ios_gamepad_physical(&remote);
        atomic_fetch_add(count, 1);
    }
    return NULL;
}
static void *enumerator(void *context) {
    atomic_long *count = context;
    uint32_t out[4];
    while (!atomic_load(&stop)) {
        assert(host_ios_gamepads(out, 4) == 1 && out[0] == 1);
        atomic_fetch_add(count, 1);
    }
    return NULL;
}
int main(void) {
    on_main = 1;
    merge.virtual_id = 1;
    (void)text_prompt_hint_had;
    uint32_t out[4];
    /* the hardware controller merges into the remote's pad */
    assert(host_ios_gamepads(out, 4) == 1 && out[0] == 1);
    assert(host_ios_gamepad_physical(&remote) == &hardware);
    assert(host_ios_gamepad_type(&remote) == SDL_GAMEPAD_TYPE_PS5);

    /* a swipe's down and up together: the remote's key is consumed and read once */
    assert(!key(SDL_SCANCODE_RETURN, true) && !key(SDL_SCANCODE_RETURN, false));
    assert(host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_SOUTH));
    assert(!host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_SOUTH));
    /* a held key reads until it is let go */
    key(SDL_SCANCODE_UP, true);
    assert(host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_DPAD_UP));
    assert(host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_DPAD_UP));
    key(SDL_SCANCODE_UP, false);
    assert(!host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_DPAD_UP));
    /* other keys pass through */
    assert(key(SDL_SCANCODE_A, true));
    /* the hardware controller's buttons and larger axes */
    atomic_store(&hardware.buttons[SDL_GAMEPAD_BUTTON_WEST], true);
    atomic_store(&hardware.axes[SDL_GAMEPAD_AXIS_LEFTX], 20000);
    assert(host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_WEST));
    assert(host_ios_gamepad_axis(&remote, SDL_GAMEPAD_AXIS_LEFTX) == 20000);

    /* the dialog up: once main has ticked, reads give nothing and remote presses are dropped */
    dialog_up = true;
    host_tv_text_prompt_tick();
    assert(text_input_blocked_cached());
    assert(!host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_WEST));
    assert(host_ios_gamepad_axis(&remote, SDL_GAMEPAD_AXIS_LEFTX) == 0);
    assert(!host_ios_gamepad_button(&hardware, SDL_GAMEPAD_BUTTON_WEST));
    assert(!key(SDL_SCANCODE_RETURN, true) && !atomic_load(&remote_latched[SDL_GAMEPAD_BUTTON_SOUTH]));
    key(SDL_SCANCODE_RETURN, false);

    /* the dialog gone with a button still held: still blocked, and a tick off main decides nothing */
    dialog_up = false;
    atomic_store(&hardware_down, true);
    host_tv_text_prompt_tick();
    assert(text_input_blocked_cached());
    atomic_store(&hardware_down, false);
    atomic_store(&remote_latched[SDL_GAMEPAD_BUTTON_SOUTH], true);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, tick_off_main, NULL) == 0 && pthread_join(thread, NULL) == 0);
    assert(text_input_blocked_cached());
    /* let go: the next tick on main gives the controller back, with the remote's latches cleared */
    host_tv_text_prompt_tick();
    assert(!text_input_blocked_cached());
    assert(!host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_SOUTH));
    assert(host_ios_gamepad_button(&remote, SDL_GAMEPAD_BUTTON_WEST));
    atomic_store(&hardware.buttons[SDL_GAMEPAD_BUTTON_WEST], false);

    /* pad reads and an enumeration on other threads while main presses, ticks and opens and closes the dialog */
    /* main waits for each thread's first pass, then ticks at least 3,000 times and until each has made two more */
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; i++)
        assert(pthread_create(&threads[i], NULL, i < READERS ? reader : enumerator, &passes.passes[i]) == 0);
    thread_passes_start(&passes, THREADS);
    int tick = 0;
    for (; thread_passes_continue(&passes, tick, 3000); tick++) {
        key(tick & 1 ? SDL_SCANCODE_DOWN : SDL_SCANCODE_RETURN, tick % 3 != 0);
        dialog_up = tick % 50 < 10;
        atomic_store(&hardware_down, tick % 50 >= 10 && tick % 50 < 15);
        atomic_store(&hardware.buttons[SDL_GAMEPAD_BUTTON_NORTH], tick % 7 == 0);
        atomic_store(&hardware.axes[SDL_GAMEPAD_AXIS_RIGHTY], (Sint16)((unsigned)tick * 10));
        host_tv_text_prompt_tick();
    }
    atomic_store(&stop, 1);
    for (int i = 0; i < THREADS; i++) assert(pthread_join(threads[i], NULL) == 0);
    long total = thread_passes_finish(&passes);
    long reads = total - atomic_load(&passes.passes[THREADS - 1]);
    printf("PASS: the Apple TV pad's remote latches and text dialog gate; %ld reads on other threads during %d ticks on main\n",
        reads, tick);
    return 0;
}
