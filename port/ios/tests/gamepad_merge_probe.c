/* Player one's merged pad (host_gamepad_merge.h) with real SDL virtual pads: the ports it gives,
   and, built with the thread sanitizer, enumerations on one thread while pad reads run on others,
   as main's pump, the game thread and the guest's own threads make them. The class: gamepad state
   shared between threads is atomic, never a plain variable one thread writes while another reads. */
#include "host_gamepad_merge.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

static struct host_gamepad_merge merge;
static SDL_JoystickID hardware[2];
/* which hardware pad the skip callback hides (-1: none); main flips it while the threads run */
static atomic_int hidden = -1;
static atomic_int stop;
static atomic_long reads, enumerations;

static int skip(uint32_t id) {
    int index = atomic_load(&hidden);
    return index >= 0 && id == hardware[index];
}

static SDL_JoystickID attach(const char *name) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.name = name;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    assert(id);
    return id;
}

static void *reader(void *context) {
    (void)context;
    while (!atomic_load(&stop)) {
        SDL_Gamepad *pad = host_gamepad_merge_primary(&merge);
        if (pad) (void)SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        atomic_fetch_add(&reads, 1);
    }
    return NULL;
}

static void *enumerator(void *context) {
    (void)context;
    uint32_t out[8];
    while (!atomic_load(&stop)) {
        int used = host_gamepad_merge_enumerate(&merge, out, 8, skip, NULL, NULL);
        assert(used >= 1 && out[0] == merge.virtual_id);
        atomic_fetch_add(&enumerations, 1);
    }
    return NULL;
}

int main(void) {
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    merge.virtual_id = attach("virtual");
    assert(SDL_OpenGamepad(merge.virtual_id));
    uint32_t out[8];
    SDL_JoystickID primary = 1, previous = 1;

    /* no hardware: the virtual pad alone, no primary */
    assert(host_gamepad_merge_enumerate(&merge, out, 8, NULL, &primary, &previous) == 1);
    assert(out[0] == merge.virtual_id && primary == 0 && previous == 0);
    assert(!host_gamepad_merge_primary(&merge));

    /* the first hardware pad merges into the virtual one (and is opened); the second takes port two */
    hardware[0] = attach("hardware 0");
    hardware[1] = attach("hardware 1");
    assert(host_gamepad_merge_enumerate(&merge, out, 8, NULL, &primary, &previous) == 2);
    assert(out[0] == merge.virtual_id && out[1] == hardware[1]);
    assert(primary == hardware[0] && previous == 0);
    assert(host_gamepad_merge_primary(&merge) == SDL_GetGamepadFromID(hardware[0]));
    assert(host_gamepad_merge_is_virtual(&merge, SDL_GetGamepadFromID(merge.virtual_id)));
    assert(!host_gamepad_merge_is_virtual(&merge, SDL_GetGamepadFromID(hardware[0])));
    /* a capacity of one keeps the virtual pad only */
    assert(host_gamepad_merge_enumerate(&merge, out, 1, NULL, NULL, NULL) == 1 && out[0] == merge.virtual_id);
    /* a skipped pad takes no port and is not the primary */
    atomic_store(&hidden, 0);
    assert(host_gamepad_merge_enumerate(&merge, out, 8, skip, &primary, &previous) == 1);
    assert(primary == hardware[1] && previous == hardware[0]);

    /* enumerations change the primary while two readers use it */
    pthread_t threads[3];
    assert(pthread_create(&threads[0], NULL, enumerator, NULL) == 0);
    assert(pthread_create(&threads[1], NULL, reader, NULL) == 0);
    assert(pthread_create(&threads[2], NULL, reader, NULL) == 0);
    for (int i = 0; i < 2000; i++) {
        atomic_store(&hidden, i % 3 - 1);
        (void)host_gamepad_merge_enumerate(&merge, out, 8, skip, NULL, NULL);
        (void)host_gamepad_merge_primary(&merge);
    }
    atomic_store(&stop, 1);
    for (int i = 0; i < 3; i++) assert(pthread_join(threads[i], NULL) == 0);
    assert(atomic_load(&reads) > 0 && atomic_load(&enumerations) > 0);

    SDL_Quit();
    printf("PASS: merged pad ports, skip and primary; %ld concurrent reads over %ld enumerations\n",
        atomic_load(&reads), atomic_load(&enumerations));
    return 0;
}
