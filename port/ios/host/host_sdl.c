/*
HOST_SDL.C

SDL3 on behalf of the guest (guest/runtime/guest_sdl.c). SDL objects are
64-bit pointers, which the guest cannot hold; it gets small handles into the
table here instead.

The guest calls these on its own threads, whose stacks remain inside the
native guest arena (host_thread.c). SDL's audio thread is the exception: it has no guest stack,
so the audio callback is handed to a thread that has one.
*/

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#if TARGET_OS_VISION
#include "host_theater.h"
#include "host_stereo.h"
#endif
#include "host.h"
#include "host_app_version.h"
#include "host_config.h"
#include "host_cloud_saves.h"
#include "host_frame_pacer.h"

#include <SDL3/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include "ios_host.h"
#if HALO_NATIVE_MAC
#include "host_macos_event.h"
#include "host_macos.h"
#endif
#include "host_join_link.h"
#include "host_external.h"
#include "host_audio_output.h"
#include "halo_host_pointer.h"
#include "host_menu_pointer.h"
#include "host_menu_gaze.h"

#define HANDLE_COUNT 256

enum handle_type
{
	_handle_free,
	_handle_window,
	_handle_context,
	_handle_gamepad,
	_handle_audio,
};

struct handle
{
	int type;
	void *object;
};

static struct handle handles[HANDLE_COUNT];
static pthread_mutex_t handle_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t handle_new(int type, void *object)
{
	uint32_t index;

	if (!object)
		return 0;
	pthread_mutex_lock(&handle_lock);
	/* an object that already has a handle keeps it */
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == type && handles[index].object == object)
		{
			pthread_mutex_unlock(&handle_lock);
			return index;
		}
	}
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == _handle_free)
		{
			handles[index].type = type;
			handles[index].object = object;
			pthread_mutex_unlock(&handle_lock);
			return index;
		}
	}
	pthread_mutex_unlock(&handle_lock);
	host_logf(HOST_LOG_ERROR, "out of SDL handles");
	return 0;
}

static void *handle_get(uint32_t handle, int type)
{
	void *object = NULL;

	if (handle == 0 || handle >= HANDLE_COUNT)
		return NULL;
	pthread_mutex_lock(&handle_lock);
	if (handles[handle].type == type)
		object = handles[handle].object;
	pthread_mutex_unlock(&handle_lock);
	return object;
}

/* ---------- general */

static void menu_pointer_hint(void *userdata, const char *name, const char *old_value, const char *value);
static void menu_pointer_event(const SDL_Event *event);
static void menu_pointer_sync(void);

int host_sdl_init(uint32_t flags)
{
	static int hinted;

	if (!hinted)
	{
		hinted = 1;
		SDL_AddHintCallback("VISR_MENU_ACTIVE", menu_pointer_hint, NULL);
	}
	return SDL_Init((SDL_InitFlags)flags);
}

int host_sdl_set_hint(const char *name, const char *value)
{
	return SDL_SetHint(name, value);
}

void host_sdl_get_error(char *buffer, uint32_t size)
{
	SDL_strlcpy(buffer, SDL_GetError(), size);
}

void host_sdl_scancode_name(int32_t scancode, char *buffer, uint32_t size)
{
	SDL_strlcpy(buffer, SDL_GetScancodeName((SDL_Scancode)scancode), size);
}

int32_t host_sdl_scancode_from_name(const char *name)
{
	return (int32_t)SDL_GetScancodeFromName(name);
}

int64_t host_sdl_ticks(void)
{
	return (int64_t)SDL_GetTicks();
}

int64_t host_sdl_thread_id(void)
{
	return (int64_t)SDL_GetCurrentThreadID();
}

/* ---------- video */

/* the Metal window's view (display.renderer = "metal"), whose layer the Metal
backend draws into (gpu_metal.m) */
static SDL_MetalView metal_view;

#if HALO_NATIVE_MAC
/* The shared ILP32 guest requests fullscreen on every Apple target. Native Mac
   defaults to a resizable window while honoring the existing explicit Mac settings. */
static SDL_WindowFlags native_window_flags(SDL_WindowFlags flags)
{
    char mode[32], fullscreen[16];
    const char *environment = getenv("HALO_DISPLAY_MODE");
    int wanted = 0;

    host_config_string("display.mode", "", mode, sizeof(mode));
    if (environment && *environment) snprintf(mode, sizeof(mode), "%s", environment);
    if (*mode)
    {
        wanted = !strcmp(mode, "fullscreen") || !strcmp(mode, "borderless");
        if (!wanted && strcmp(mode, "windowed"))
            host_logf(HOST_LOG_WARN, "Unknown native display.mode %s; using a window", mode);
    }
    else
    {
        environment = getenv("HALO_FULLSCREEN");
        host_config_string("display.fullscreen", "false", fullscreen, sizeof(fullscreen));
        if (environment) snprintf(fullscreen, sizeof(fullscreen), "%s", environment);
        wanted = !strcmp(fullscreen, "true") || !strcmp(fullscreen, "1");
    }
    flags = (flags & ~SDL_WINDOW_FULLSCREEN) | SDL_WINDOW_RESIZABLE;
    return wanted ? flags | SDL_WINDOW_FULLSCREEN : flags;
}
#endif

uint32_t host_sdl_create_window(const char *title, int width, int height, int64_t flags)
{
#if HALO_NATIVE_MAC
    SDL_WindowFlags requested = (SDL_WindowFlags)flags;
    flags = (int64_t)native_window_flags(requested);
    /* The shared guest supplies its original title; the native app has its own name. */
    title = "VISR";
    host_logf(HOST_LOG_INFO, "Native SDL window flags: requested 0x%llx, effective 0x%llx (%s)",
        (unsigned long long)requested, (unsigned long long)flags,
        (flags & SDL_WINDOW_FULLSCREEN) ? "explicit fullscreen" : "resizable window");
#endif
	SDL_Window *window = SDL_CreateWindow(title, width, height, (SDL_WindowFlags)flags);

	/* a GL window gets its view, and then its touch controls, from
	host_sdl_gl_create_context. A Metal window has no context, so both
	happen here, in the same order: SDL_Metal_CreateView replaces the
	window's root view, which would drop controls attached before it */
	if (window && (flags & SDL_WINDOW_METAL))
	{
		metal_view = SDL_Metal_CreateView(window);
		if (!metal_view)
			host_logf(HOST_LOG_ERROR, "SDL_Metal_CreateView failed: %s", SDL_GetError());
		host_ios_touch_attach(window);
        host_external_attach(window,1);
	}
	return handle_new(_handle_window, window);
}

void *host_sdl_metal_layer(void)
{
	return metal_view ? SDL_Metal_GetLayer(metal_view) : NULL;
}

static int pinned_width, pinned_height, output_capped, output_cap_requested, native_width, native_height;

void host_sdl_pin_window_pixels(int width, int height)
{
	pinned_width = width;
	pinned_height = height;
}

void host_sdl_output_native(int width, int height, int requested_cap)
{
	native_width = width;
	native_height = height;
	output_cap_requested = requested_cap > 0 ? requested_cap : 0;
}

void host_sdl_native_output(int *width, int *height)
{
	*width = native_width;
	*height = native_height;
}

int host_sdl_output_cap_requested(void)
{
	return output_cap_requested;
}

void host_sdl_cap_output(int width, int height)
{
	host_sdl_pin_window_pixels(width, height);
	output_capped = 1;
}

void host_sdl_uncap_output(void)
{
	if (output_capped)
		host_sdl_pin_window_pixels(0, 0);
	output_capped = 0;
}

int host_sdl_output_cap(int *width, int *height)
{
	*width = output_capped ? pinned_width : 0;
	*height = output_capped ? pinned_height : 0;
	return output_capped;
}

int host_sdl_display_max_fps(void)
{
#if HALO_NATIVE_MAC
	return (int)host_macos_refresh_rate();
#else
	return 0;
#endif
}

void host_sdl_window_size_in_pixels(uint32_t window, int *width, int *height)
{
	SDL_Window *object = handle_get(window, _handle_window);

	*width = 0;
	*height = 0;
#if TARGET_OS_VISION
	/* theater mode: the game renders for the screen in the room, not for its
	hidden window (host_theater.m); in head-tracked stereo, for the eyes'
	views (host_stereo.m) */
	if (object && host_theater_active())
	{
		if (!host_stereo_picture_size(width, height))
			host_theater_picture_size(width, height);
		return;
	}
#endif
    if(object && host_external_size(width,height))return;
	if (object && pinned_width)
	{
		*width = pinned_width;
		*height = pinned_height;
		return;
	}
	if (object)
		SDL_GetWindowSizeInPixels(object, width, height);
}

int host_sdl_set_relative_mouse(uint32_t window, int enabled)
{
	SDL_Window *object = handle_get(window, _handle_window);

	return object ? SDL_SetWindowRelativeMouseMode(object, enabled != 0) : 0;
}

int host_sdl_gl_set_attribute(int attribute, int value)
{
	/* EAGL exposes ES 3.0; capabilities are queried from the created context. */
	if (attribute == SDL_GL_CONTEXT_MINOR_VERSION) value = 0;
	return SDL_GL_SetAttribute((SDL_GLAttr)attribute, value);
}

uint32_t host_sdl_gl_create_context(uint32_t window)
{
	SDL_Window *object = handle_get(window, _handle_window);
	SDL_GLContext context = object ? SDL_GL_CreateContext(object) : NULL;
	if (context) { host_ios_touch_attach(object);host_external_attach(object,0); }
	return handle_new(_handle_context, context);
}

int host_sdl_gl_make_current(uint32_t window, uint32_t context)
{
	return SDL_GL_MakeCurrent(handle_get(window, _handle_window), handle_get(context, _handle_context));
}

int host_sdl_gl_set_swap_interval(int interval)
{
	return SDL_GL_SetSwapInterval(interval);
}

int host_sdl_gl_swap_window(uint32_t window)
{
	SDL_Window *object = handle_get(window, _handle_window);

	return object ? SDL_GL_SwapWindow(object) : 0;
}

/* ---------- events */

/* host_main.m: a second window scene's start, undone (see there); and its
test */
void host_extra_scene_poll(void);
void host_extra_scene_test(void);

/* (the system's keyboard, below) */
static void text_input_update(void);
/* while a share or audio output sheet is up the player's presses and
movements are the sheet's, not the game's: the game gets the releases alone,
so nothing stays held */
static int held_back(const SDL_Event *event)
{
	if (!host_window_up() && !host_audio_output_up())
		return 0;
	switch (event->type)
	{
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_TEXT_INPUT:
	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_WHEEL:
	case SDL_EVENT_FINGER_DOWN:
	case SDL_EVENT_FINGER_MOTION:
	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		return 1;
	default:
		return 0;
	}
}

int host_sdl_poll_event(void *event)
{
	SDL_Event host_event;
	static int test_extra_scene = -1;
	static unsigned long polls;

	menu_pointer_sync();
	host_extra_scene_poll();
    host_external_poll();
    host_audio_output_poll();
#if TARGET_OS_VISION
	if (test_extra_scene < 0)
	{
		char value[16];

		host_config_string("debug.test_extra_scene", "false", value, sizeof(value));
		test_extra_scene = !strcmp(value, "true");
	}
	/* a few seconds in, once the game's window is up */
	if (test_extra_scene && ++polls == 600)
		host_extra_scene_test();
	/* and later, that the host still reads config.toml */
	if (test_extra_scene && polls == 1800)
		host_logf(HOST_LOG_INFO, "debug.test_extra_scene: config.toml's display.theater_distance reads %.2f",
			host_config_real("display.theater_distance", -1.0));
	{
		int count = 0, held = 0;
		SDL_JoystickID *ids = SDL_GetGamepads(&count);

		for (int index = 0; ids && index < count && !held; index++)
		{
			SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[index]);
			held = gamepad && SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK) &&
				SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
		}
		SDL_free(ids);
		host_theater_chord(held || host_spatial_sense_menus_held());
	}
	/* debug.test_theater_reopen: Exit VR if the space is open, else Enter VR,
	every 900 polls, six times */
	{
		static int test_theater_reopen = -1;
		static unsigned long theater_polls;

		if (test_theater_reopen < 0)
		{
			char value[16];

			host_config_string("debug.test_theater_reopen", "false", value, sizeof(value));
			test_theater_reopen = !strcmp(value, "true");
		}
		if (test_theater_reopen && ++theater_polls % 900 == 0 && theater_polls <= 5400)
			host_theater_test_toggle();
	}
#else
	(void)test_extra_scene;
	(void)polls;
#endif
	text_input_update();
	/* (a malformed join link's message, between frames: host_join_link.c) */
	host_join_link_poll();
	do
	{
		if (!SDL_PollEvent(&host_event))
			return 0;
	}
	while (held_back(&host_event));
	menu_pointer_event(&host_event);
#if HALO_NATIVE_MAC
    host_macos_normalize_event(&host_event);
#endif
#if HALO_FRAME_PACING && (TARGET_OS_MACCATALYST || HALO_NATIVE_MAC)
	/* the Mac app's F8: the frame pacer's late start on and off while
	playing, to feel the difference (display.late_start) */
	if (host_event.type == SDL_EVENT_KEY_DOWN && !host_event.key.repeat && host_event.key.scancode == SDL_SCANCODE_F8)
		frame_pacer_late_start_toggle();
#endif
	/* the layouts agree except for the pointers of text, drop and user
	events, which the guest does not read */
	memcpy(event, &host_event, sizeof(host_event));
	return 1;
}

/* ---------- gamepads */

static atomic_int rumble_held;

int host_sdl_controller_aim_owns(uint32_t id)
{
	const char *name = SDL_GetGamepadNameForID((SDL_JoystickID)id);
	return name && strstr(name, "VR2 Sense") && host_spatial_owns_sense_controllers();
}

void host_sdl_rumble_hold(int hold)
{
	int count = 0;
	SDL_JoystickID *ids;

	atomic_store(&rumble_held, hold != 0);
	if (!hold)
		return;
	ids = SDL_GetGamepads(&count);
	for (int index = 0; ids && index < count; index++)
	{
		SDL_Gamepad *gamepad = SDL_GetGamepadFromID(ids[index]);
		if (gamepad)
			SDL_RumbleGamepad(gamepad, 0, 0, 0);
	}
	SDL_free(ids);
}

int host_sdl_get_gamepads(uint32_t *ids, int capacity)
{
	return host_ios_gamepads(ids, capacity);
}

uint32_t host_sdl_open_gamepad(uint32_t id)
{
	SDL_Gamepad *gamepad = SDL_OpenGamepad((SDL_JoystickID)id);

	if (gamepad)
		host_logf(HOST_LOG_INFO, "gamepad %u: %s (type %d, %04x:%04x)", (unsigned)id, SDL_GetGamepadName(gamepad),
			(int)SDL_GetGamepadType(gamepad), SDL_GetGamepadVendor(gamepad), SDL_GetGamepadProduct(gamepad));
	return handle_new(_handle_gamepad, gamepad);
}

uint32_t host_sdl_gamepad_from_id(uint32_t id)
{
	return handle_new(_handle_gamepad, SDL_GetGamepadFromID((SDL_JoystickID)id));
}

int host_sdl_gamepad_axis(uint32_t gamepad, int axis)
{
	SDL_Gamepad *object = handle_get(gamepad, _handle_gamepad);

	/* (at rest while a share sheet is up) */
	return object && !host_window_up() && !host_audio_output_up() ? host_ios_gamepad_axis(object, axis) : 0;
}

int host_sdl_gamepad_button(uint32_t gamepad, int button)
{
	SDL_Gamepad *object = handle_get(gamepad, _handle_gamepad);

	/* (none pressed while a share sheet is up) */
	return object && !host_window_up() && !host_audio_output_up() ? host_ios_gamepad_button(object, button) : 0;
}

int host_sdl_gamepad_type(uint32_t gamepad)
{
	SDL_Gamepad *object = handle_get(gamepad, _handle_gamepad);

	return object ? host_ios_gamepad_type(object) : SDL_GAMEPAD_TYPE_UNKNOWN;
}

int host_sdl_rumble_gamepad(uint32_t gamepad, uint32_t low, uint32_t high, uint32_t milliseconds)
{
	SDL_Gamepad *object = handle_get(gamepad, _handle_gamepad);

	if (atomic_load(&rumble_held))
		low = high = milliseconds = 0;
	return object ? SDL_RumbleGamepad(object, (Uint16)low, (Uint16)high, milliseconds) : 0;
}

/* ---------- audio */

/* SDL holds its stream lock while calling audio_callback. The guest worker
must stage its PCM here: calling SDL_PutAudioStreamData from that worker would
wait for the lock held by the callback that is waiting for the worker. Only
the original SDL callback thread submits the staged samples to the stream. */
struct audio_binding
{
	uint32_t handle;
	uint32_t callback;
	uint32_t userdata;
	pthread_mutex_t lock;
	pthread_cond_t requested;
	pthread_cond_t done;
	int pending;
	int additional;
	int total;
	unsigned char *samples;
	int length;
	int capacity;
	SDL_AudioFormat format;
	int logged_output;
	/* how long the guest takes to mix (audio_callback_timing) */
	double bytes_per_second;
	Uint64 timing_since;
	unsigned long timing_callbacks, timing_late;
	double timing_slowest_ms, timing_slowest_sound_ms;
	/* SDL_GetTicksNS at the latest callback, or when the stream was opened or resumed (host_sdl_audio_stalled) */
	_Atomic Uint64 last_callback_ns;
	/* the guest's format, which every reopen keeps; and, while a reopen has failed and the slot has no stream,
	whether it was playing (host_sdl_audio_route_changed) */
	SDL_AudioSpec source;
	bool playing_when_lost;
};

/* Every 30 seconds, how long the guest took to mix a callback's sound against how long that sound plays: a mix
slower than its sound falls behind the device, which then runs dry */
static void audio_callback_timing(struct audio_binding *binding, int additional, Uint64 began, Uint64 ended)
{
	double mix_ms = (double)(ended - began) / 1.0e6;
	double sound_ms = binding->bytes_per_second > 0.0 ? 1000.0 * (double)additional / binding->bytes_per_second : 0.0;

	if (!binding->timing_since)
		binding->timing_since = began;
	binding->timing_callbacks++;
	if (sound_ms > 0.0 && mix_ms > sound_ms)
		binding->timing_late++;
	if (mix_ms > binding->timing_slowest_ms)
	{
		binding->timing_slowest_ms = mix_ms;
		binding->timing_slowest_sound_ms = sound_ms;
	}
	if (ended - binding->timing_since >= 30 * SDL_NS_PER_SECOND)
	{
		host_logf(HOST_LOG_INFO, "audio mix: %lu callbacks in %.0f s, %lu slower than the sound they made; slowest %.2f ms for %.2f ms of sound",
			binding->timing_callbacks, (double)(ended - binding->timing_since) / 1.0e9, binding->timing_late,
			binding->timing_slowest_ms, binding->timing_slowest_sound_ms);
		binding->timing_since = ended;
		binding->timing_callbacks = binding->timing_late = 0;
		binding->timing_slowest_ms = binding->timing_slowest_sound_ms = 0.0;
	}
}

static __thread struct audio_binding *audio_worker_binding;

static void *audio_thread(void *context)
{
	struct audio_binding *binding = context;

	audio_worker_binding = binding;
	pthread_mutex_lock(&binding->lock);
	for (;;)
	{
		int additional, total;

		while (!binding->pending)
			pthread_cond_wait(&binding->requested, &binding->lock);
		additional = binding->additional;
		total = binding->total;
		pthread_mutex_unlock(&binding->lock);
		host_call_guest(binding->callback, binding->userdata, binding->handle, (uint32_t)additional, (uint32_t)total);
		pthread_mutex_lock(&binding->lock);
		binding->pending = 0;
		pthread_cond_signal(&binding->done);
	}
	return NULL;
}

static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
	struct audio_binding *binding = userdata;
	Uint64 began = SDL_GetTicksNS();

	atomic_store(&binding->last_callback_ns, began);

	pthread_mutex_lock(&binding->lock);
	binding->length = 0;
	binding->additional = additional;
	binding->total = total;
	binding->pending = 1;
	pthread_cond_signal(&binding->requested);
	while (binding->pending)
		pthread_cond_wait(&binding->done, &binding->lock);
	audio_callback_timing(binding, additional, began, SDL_GetTicksNS());
	if (binding->length && !SDL_PutAudioStreamData(stream, binding->samples, binding->length))
		host_logf(HOST_LOG_ERROR, "cannot submit audio: %s", SDL_GetError());
	if (!binding->logged_output && binding->format == SDL_AUDIO_F32)
	{
		const float *samples = (const float *)binding->samples;
		float peak = 0.0f;
		for (int i = 0; i < binding->length / (int)sizeof(float); ++i)
			peak = SDL_max(peak, SDL_fabsf(samples[i]));
		if (peak > 0.0001f)
		{
			host_logf(HOST_LOG_INFO, "audio output active: %d PCM bytes, peak %.4f", binding->length, peak);
			binding->logged_output = 1;
		}
	}
	pthread_mutex_unlock(&binding->lock);
}

/* the open audio streams, which host_sdl_audio_pause stops while the app is
in the background (host_lifecycle.m) */
static SDL_AudioStream *audio_streams[4];
static struct audio_binding *audio_bindings[4];
/* a reopen failed: host_sdl_audio_stalled says so, so the caller tries again */
static int audio_retry;

/* Keep the guest's source format stable. SDL converts it to the new route's
   device rate; changing the source rate would change the game's pitch.

   Every stream is closed before any is opened again. SDL keeps one physical
   device (on iOS, one AudioQueue) open while any stream is bound to it, so a
   stream opened before the old one closed joined the same queue, and a queue
   the system had stopped (an interruption, or a route change such as an HDMI
   display connecting mid-game) stayed silent through every later reopen. Only
   a physical device closed and opened again gets a new queue on the current
   route at its current rate and channel count. */
void host_sdl_audio_route_changed(void)
{
    SDL_AudioStream *old[SDL_arraysize(audio_streams)]={0};
    bool paused[SDL_arraysize(audio_streams)];
    int count=0;
    for(size_t i=0;i<SDL_arraysize(audio_streams);i++) {
        if(!audio_bindings[i])continue;
        old[i]=audio_streams[i];
        /* (a slot whose last reopen failed has no stream: it is opened again as it was) */
        if(!old[i]){paused[i]=!audio_bindings[i]->playing_when_lost;count++;continue;}
        /* (pausing waits for a callback under way, so none runs while the streams change) */
        paused[i]=SDL_AudioStreamDevicePaused(old[i]);
        SDL_PauseAudioStreamDevice(old[i]);
        count++;
    }
    if(!count)return;
    audio_retry=0;
    /* the guest's calls find no stream until the replacement is in place (a put is dropped, as into a paused stream) */
    pthread_mutex_lock(&handle_lock);
    for(size_t i=0;i<SDL_arraysize(audio_streams);i++)
        if(old[i]) {handles[audio_bindings[i]->handle].object=NULL;audio_streams[i]=NULL;}
    pthread_mutex_unlock(&handle_lock);
    for(size_t i=0;i<SDL_arraysize(audio_streams);i++)
        if(old[i])SDL_DestroyAudioStream(old[i]);
    for(size_t i=0;i<SDL_arraysize(audio_streams);i++) {
        struct audio_binding *binding=audio_bindings[i];
        if(!binding)continue;
        SDL_AudioStream *replacement=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&binding->source,
            binding->callback?audio_callback:NULL,binding);
        if(!replacement) {
            /* (the slot stays bound to its handle with no stream: host_sdl_audio_stalled asks for another try) */
            host_logf(HOST_LOG_ERROR,"audio route: cannot reopen stream: %s",SDL_GetError());
            binding->playing_when_lost=!paused[i];
            audio_retry=1;
            continue;
        }
        pthread_mutex_lock(&handle_lock);
        handles[binding->handle].object=replacement;
        audio_streams[i]=replacement;
        pthread_mutex_unlock(&handle_lock);
        atomic_store(&binding->last_callback_ns,SDL_GetTicksNS());
        if(!paused[i])SDL_ResumeAudioStreamDevice(replacement);
        SDL_AudioSpec output;
        if(SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(replacement),&output,NULL))
            host_logf(HOST_LOG_INFO,"audio route: stream reopened on a new device at %d Hz, %d channels (guest source %d Hz)",
                output.freq,output.channels,binding->source.freq);
    }
}

/* A stream that should be playing whose callbacks stopped coming: the system stopped the device without a
   notification that reached SDL or the route check (an interruption whose end was never sent, a route change
   while the app was in the background). now: SDL_GetTicksNS. 1 when a stream has been silent for over 1.5 s. */
int host_sdl_audio_stalled(Uint64 now)
{
    int stalled=0;
    for(size_t i=0;i<SDL_arraysize(audio_streams);i++) {
        struct audio_binding *binding=audio_bindings[i];
        if(!binding || !binding->callback)continue;
        /* (a reopen that failed) */
        if(!audio_streams[i])continue;
        if(SDL_AudioStreamDevicePaused(audio_streams[i]))continue;
        Uint64 last=atomic_load(&binding->last_callback_ns);
        if(last && now>last && now-last>1500*SDL_NS_PER_MS)stalled=1;
    }
    return stalled || audio_retry;
}

/* every stream's stall clock starts now (a system interruption has ended: host_sdl_audio_stalled) */
void host_sdl_audio_touch(void)
{
	size_t index;

	for (index = 0; index < SDL_arraysize(audio_bindings); index++)
		if (audio_bindings[index])
			atomic_store(&audio_bindings[index]->last_callback_ns, SDL_GetTicksNS());
}

void host_sdl_audio_pause(int paused)
{
	size_t index;

	for (index = 0; index < SDL_arraysize(audio_streams); index++)
		if (audio_streams[index])
		{
			if (paused)
				SDL_PauseAudioStreamDevice(audio_streams[index]);
			else
			{
				if (audio_bindings[index])
					atomic_store(&audio_bindings[index]->last_callback_ns, SDL_GetTicksNS());
				SDL_ResumeAudioStreamDevice(audio_streams[index]);
			}
		}
}

uint32_t host_sdl_open_audio_stream(uint32_t device, const void *spec, uint32_t callback, uint32_t userdata)
{
	struct audio_binding *binding = SDL_calloc(1, sizeof(*binding));
	SDL_AudioStream *stream;

	if (!binding)
		return 0;
	binding->callback = callback;
	binding->userdata = userdata;
	binding->format = ((const SDL_AudioSpec *)spec)->format;
	binding->source = *(const SDL_AudioSpec *)spec;
	binding->bytes_per_second = (double)((const SDL_AudioSpec *)spec)->freq * ((const SDL_AudioSpec *)spec)->channels *
		SDL_AUDIO_BYTESIZE(binding->format);
	pthread_mutex_init(&binding->lock, NULL);
	pthread_cond_init(&binding->requested, NULL);
	pthread_cond_init(&binding->done, NULL);
	stream = SDL_OpenAudioDeviceStream((SDL_AudioDeviceID)device, spec,
		callback ? audio_callback : NULL, binding);
	if (!stream)
	{
		pthread_cond_destroy(&binding->done);
		pthread_cond_destroy(&binding->requested);
		pthread_mutex_destroy(&binding->lock);
		SDL_free(binding);
		return 0;
	}
	/* the host's audio device, one of the host inputs that reaches game state (the guest mixes
	on its callback's cadence): compared between runners by tools/mac_run.py compare-inputs */
	{
		SDL_AudioSpec device_spec;
		int device_frames = 0;
		if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &device_spec, &device_frames))
			host_logf(HOST_LOG_INFO, "audio device: %d Hz, %d channels, format 0x%x, %d sample frames",
				device_spec.freq, device_spec.channels, (unsigned)device_spec.format, device_frames);
	}
	/* the device starts paused, so no callback can run before this */
	binding->handle = handle_new(_handle_audio, stream);
	{
		size_t index;

		for (index = 0; index < SDL_arraysize(audio_streams); index++)
			if (!audio_streams[index])
			{
				audio_streams[index] = stream;
                audio_bindings[index]=binding;
				break;
			}
	}
	if (callback && host_native_thread_create(audio_thread, binding, 256 * 1024) != 0)
		host_crash("cannot start the audio thread");
	return binding->handle;
}

int host_sdl_put_audio_stream_data(uint32_t stream, const void *data, int length)
{
	struct audio_binding *binding = audio_worker_binding;

	if (binding && binding->handle == stream)
	{
		/* The callback is asleep until this worker signals completion. Copy
		   now because the guest's mix buffer is on its stack and is reused. */
		if (length < 0 || (!data && length) || length > INT_MAX - binding->length)
			return SDL_SetError("invalid guest audio buffer");
		int needed = binding->length + length;
		if (needed > binding->capacity)
		{
			int capacity = SDL_max(needed, 65536);
			void *samples = SDL_realloc(binding->samples, (size_t)capacity);
			if (!samples)
				return SDL_OutOfMemory();
			binding->samples = samples;
			binding->capacity = capacity;
		}
		if (length)
			SDL_memcpy(binding->samples + binding->length, data, (size_t)length);
		binding->length = needed;
		return 1;
	}
	SDL_AudioStream *object = handle_get(stream, _handle_audio);

	return object ? SDL_PutAudioStreamData(object, data, length) : 0;
}

int host_sdl_resume_audio_stream_device(uint32_t stream)
{
	SDL_AudioStream *object = handle_get(stream, _handle_audio);
	size_t index;

	/* (a stream paused for a while is not stalled the moment it resumes: host_sdl_audio_stalled) */
	for (index = 0; index < SDL_arraysize(audio_bindings); index++)
		if (audio_bindings[index] && audio_bindings[index]->handle == stream)
			atomic_store(&audio_bindings[index]->last_callback_ns, SDL_GetTicksNS());
	return object ? SDL_ResumeAudioStreamDevice(object) : 0;
}

/* ---------- the clipboard (internet play's invite links) */

int host_sdl_set_clipboard_text(const char *text)
{
	return SDL_SetClipboardText(text) ? 1 : 0;
}

void host_sdl_get_clipboard_text(char *buffer, uint32_t size)
{
	char *text = SDL_GetClipboardText();

	SDL_strlcpy(buffer, text ? text : "", size);
	SDL_free(text);
}

/* ---------- the system's keyboard (a menu's text field: a join code, a
password, a game's name)

On a phone, a tablet or a headset the keyboard is the system's, on screen,
up while the field is edited. SDL turns what is typed on it into key presses
(SDL_uikitviewcontroller.m's SDL_SendKeyboardUnicodeKey, shift held for a
capital), which the guest reads as a hardware keyboard's; its Return is a
Return key press. On an Apple TV SDL's hidden text field brings no keyboard
up, so the field is asked for with tvOS's own dialog (host_tv.m), whose text
the guest takes whole (host_sdl_text_result). Either is begun in
host_sdl_poll_event, on the main thread, which UIKit needs. */

static int text_input_wanted, text_input_on, text_input_masked;
static char text_input_title[64], text_input_text[512];

void host_sdl_text_input(int on, const char *title, const char *text, int masked)
{
	if (on)
	{
		SDL_strlcpy(text_input_title, title ? title : "", sizeof(text_input_title));
		SDL_strlcpy(text_input_text, text ? text : "", sizeof(text_input_text));
		text_input_masked = masked != 0;
	}
	text_input_wanted = on != 0;
}

/* ---------- fingers and pointer devices in the menus (halo_host_pointer.h, host_menu_pointer.h)

One state for them all: the fingers (host_menu_pointer.m) and a mouse or
trackpad (SDL's mouse events, read in host_sdl_poll_event) add to it, and the
guest's host_ui_pointer_read takes what they did. It reports a source on every
host with a screen to touch or a mouse to move; the Apple TV and the Vision
Pro have none, and their menus stay as they were. */

static struct host_menu_pointer menu_pointer;
static void (*pointer_feedback)(int);
static void (*pointer_menus_changed)(int);

struct host_menu_pointer *host_menu_pointer_shared(void)
{
	return &menu_pointer;
}

void host_ui_pointer_install(void (*feedback)(int), void (*menus_changed)(int))
{
	pointer_feedback = feedback;
	pointer_menus_changed = menus_changed;
}

/* the guest publishes VISR_MENU_ACTIVE with the menus (halo_ui_pointer_update). SDL calls this with its hints locked,
so it only notes the value; menu_pointer_sync takes it up on the next poll or read, on the main thread */
static atomic_int menu_pointer_hinted = -1;

static void menu_pointer_hint(void *userdata, const char *name, const char *old_value, const char *value)
{
	int menus = value && (value[0] == '1' || !SDL_strcasecmp(value, "true"));

	(void)userdata;
	(void)name;
	(void)old_value;
	atomic_store(&menu_pointer_hinted, menus);
}

static void menu_pointer_sync(void)
{
	int menus = atomic_exchange(&menu_pointer_hinted, -1);

	if (menus < 0)
		return;
	host_menu_pointer_set_menus(&menu_pointer, menus);
	if (pointer_menus_changed)
		pointer_menus_changed(menus);
}

void host_menu_pointer_block(int blocked)
{
	host_menu_pointer_set_blocked(&menu_pointer, blocked);
}

static void menu_pointer_event(const SDL_Event *event)
{
	static float wheel;
	SDL_Window *window;
	int width = 0, height = 0;

#if TARGET_OS_VISION
	/* the Vision Pro's menus are on a surface in the room, not in the window a mouse moves in; a look and pinch
	drives them (host_menu_gaze_spatial_event) */
	(void)event;
	(void)wheel;
	(void)window;
	(void)width;
	(void)height;
	return;
#endif
	switch (event->type)
	{
	case SDL_EVENT_MOUSE_MOTION:
		if (event->motion.which == SDL_TOUCH_MOUSEID || event->motion.which == SDL_PEN_MOUSEID)
			return;
		window = SDL_GetWindowFromID(event->motion.windowID);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (event->button.which == SDL_TOUCH_MOUSEID || event->button.which == SDL_PEN_MOUSEID || !event->button.down)
			return;
		window = SDL_GetWindowFromID(event->button.windowID);
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		window = SDL_GetWindowFromID(event->wheel.windowID);
		break;
	default:
		return;
	}
	if (window && SDL_GetWindowSize(window, &width, &height))
		host_menu_pointer_set_surface(&menu_pointer, (float)width, (float)height);
	switch (event->type)
	{
	case SDL_EVENT_MOUSE_MOTION:
		host_menu_pointer_device_move(&menu_pointer, event->motion.x, event->motion.y);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (event->button.button == SDL_BUTTON_LEFT || event->button.button == SDL_BUTTON_RIGHT)
			host_menu_pointer_device_click(&menu_pointer, event->button.button == SDL_BUTTON_RIGHT,
				event->button.x, event->button.y);
		break;
	default:
		wheel += event->wheel.y;
		while (wheel >= 1.0f)
		{
			host_menu_pointer_device_wheel(&menu_pointer, 1);
			wheel -= 1.0f;
		}
		while (wheel <= -1.0f)
		{
			host_menu_pointer_device_wheel(&menu_pointer, -1);
			wheel += 1.0f;
		}
		break;
	}
}

/* ---------- look and pinch on the Vision Pro's menus (host_menu_gaze.h)

The presenters note where they drew the menus' surface (host_stereo.m, host_theater.m); Theater.swift hands each
spatial event of the immersive space to host_menu_gaze_spatial_event. Both run on the main thread, as the game
does. */

static struct host_menu_gaze_surface gaze_surface;
static struct host_menu_gaze gaze;

static double gaze_now(void)
{
	return (double)SDL_GetTicksNS() / 1e9;
}

void host_menu_gaze_note_surface(const float frame_to_origin[16], const float center[3], const float x_axis[3],
	const float y_axis[3], const float eye[3])
{
	host_menu_gaze_surface_set(&gaze_surface, frame_to_origin, center, x_axis, y_axis, eye, gaze_now());
}

static const struct host_menu_gaze_surface *gaze_surface_drawn(void)
{
	return host_menu_gaze_surface_fresh(&gaze_surface, gaze_now()) ? &gaze_surface : NULL;
}

void host_menu_gaze_spatial_event(intptr_t id, int phase, int has_ray, const float *ray, int has_hand,
	const float *hand)
{
	int tracking = gaze.tracking, taken;

	menu_pointer_sync();
	taken = host_menu_gaze_event(&gaze, &menu_pointer, gaze_surface_drawn(), id, phase, has_ray ? ray : NULL,
		has_hand ? hand : NULL);
	/* each pinch the menus take, as it begins and ends (the move between is the hand's travel) */
	if (taken && !tracking)
		host_logf(HOST_LOG_INFO, "menu pointer: a pinch at %.0f, %.0f of %.0f by %.0f points on the menus",
			gaze.down_x, gaze.down_y, menu_pointer.surface_width, menu_pointer.surface_height);
	else if (tracking && !gaze.tracking)
		host_logf(HOST_LOG_INFO, "menu pointer: the pinch %s", phase == HOST_MENU_GAZE_CANCELLED ? "was canceled" :
			menu_pointer.left_clicks ? "let go without travel: a press" : "let go after a drag");
}

/* HALO_MENU_POINTER_SCRIPT, for runs with no finger: ops separated by ';', each "FRAME KIND ARGS", where FRAME counts
the frames a menu has been up (reads), and positions are fractions of the window: "60 tap 0.5 0.4", "90 drag 0.5 0.7
0.5 0.3", "120 click 0.5 0.4", "130 right 0.5 0.4", "140 wheel 2". A tap is a finger down, then up on the next frame;
a drag moves in six steps. On the Vision Pro, "150 pinch 0.5 0.4" looks at that fraction of the menus' surface in
the room (from where the eyes were when it was drawn) and pinches, letting go on the next frame or after a count of
frames given after the position ("150 pinch 0.5 0.4 90"), and "160 pinchdrag 0.5 0.4 0 6" pinches there and
moves the hand 0 cm right and 6 cm down in six steps before letting go: synthetic spatial events through the same
path as the headset's (host_menu_gaze_spatial_event), because the simulator cannot make them. With no surface drawn
in the room (the game in its window) a pinch is a tap on the window. Each op is logged, so a journey can be checked
against the log and the screenshots. */
static struct
{
	int loaded;
	const char *cursor;
	unsigned long frame;
	/* the op running */
	int kind, step, steps;
	unsigned long start;
	float x0, y0, x1, y1;
} menu_script;

static void menu_script_next(void)
{
	char kind[16];
	unsigned long frame;
	int used = 0;

	menu_script.kind = 0;
	while (menu_script.cursor && *menu_script.cursor == ';')
		menu_script.cursor++;
	if (!menu_script.cursor || !*menu_script.cursor)
		return;
	menu_script.x0 = menu_script.y0 = menu_script.x1 = menu_script.y1 = 0.0f;
	if (sscanf(menu_script.cursor, "%lu %15s%n", &frame, kind, &used) < 2)
	{
		menu_script.cursor = NULL;
		return;
	}
	menu_script.cursor += used;
	menu_script.start = frame;
	menu_script.step = 0;
	menu_script.steps = 1;
	if (!strcmp(kind, "tap") || !strcmp(kind, "click") || !strcmp(kind, "right"))
	{
		menu_script.kind = !strcmp(kind, "tap") ? 1 : !strcmp(kind, "click") ? 3 : 4;
		sscanf(menu_script.cursor, "%f %f%n", &menu_script.x0, &menu_script.y0, &used);
	}
	else if (!strcmp(kind, "drag"))
	{
		menu_script.kind = 2;
		menu_script.steps = 8;
		sscanf(menu_script.cursor, "%f %f %f %f%n", &menu_script.x0, &menu_script.y0, &menu_script.x1, &menu_script.y1,
			&used);
	}
	else if (!strcmp(kind, "wheel"))
	{
		menu_script.kind = 5;
		sscanf(menu_script.cursor, "%f%n", &menu_script.x0, &used);
	}
	else if (!strcmp(kind, "pinch"))
	{
		int hold = 0, held = 0;

		menu_script.kind = 6;
		sscanf(menu_script.cursor, "%f %f%n", &menu_script.x0, &menu_script.y0, &used);
		/* (an optional count of frames to hold the pinch before letting go) */
		if (sscanf(menu_script.cursor + used, " %d%n", &hold, &held) == 1 && hold > 1)
		{
			menu_script.steps = hold;
			used += held;
		}
	}
	else if (!strcmp(kind, "pinchdrag"))
	{
		menu_script.kind = 7;
		menu_script.steps = 8;
		sscanf(menu_script.cursor, "%f %f %f %f%n", &menu_script.x0, &menu_script.y0, &menu_script.x1, &menu_script.y1,
			&used);
	}
	menu_script.cursor += used;
	host_logf(HOST_LOG_INFO, "menu pointer script: frame %lu %s %.3f %.3f %.3f %.3f", frame, kind, menu_script.x0,
		menu_script.y0, menu_script.x1, menu_script.y1);
}

/* a scripted pinch's step on the menus' surface in the room; 0 when none is drawn (the op is then the window's) */
static int menu_script_pinch(void)
{
	const struct host_menu_gaze_surface *surface = gaze_surface_drawn();
	const intptr_t pinch = 9001;
	float ray[6], hand[3], t;
	int phase = HOST_MENU_GAZE_ACTIVE, last = menu_script.steps;

	if (!surface && menu_script.step == 0)
	{
		host_logf(HOST_LOG_INFO, "menu pointer script: no menu surface in the room; the pinch is a tap on the window");
		menu_script.kind = menu_script.kind == 6 ? 1 : 2;
		/* (a drag on the window from the point to the point plus the travel, as fractions: a centimeter for 2%) */
		menu_script.x1 = menu_script.x0 + menu_script.x1 * 0.02f;
		menu_script.y1 = menu_script.y0 + menu_script.y1 * 0.02f;
		return 0;
	}
	if (menu_script.step == last)
		phase = HOST_MENU_GAZE_ENDED;
	if (!surface)
	{
		/* (the surface went away mid-pinch: its end still arrives) */
		host_menu_gaze_spatial_event(pinch, phase, 0, NULL, 0, NULL);
		menu_script.step++;
		if (menu_script.step > last)
			menu_script_next();
		return 1;
	}
	/* the eyes look at the fraction of the surface from where they were; the hand rests 30 cm below them and 30 cm
	along the look, and moves along the surface's width and height */
	{
		float width = sqrtf(host_menu_gaze_dot(surface->x_axis, surface->x_axis));
		float height = sqrtf(host_menu_gaze_dot(surface->y_axis, surface->y_axis)), length;

		for (int i = 0; i < 3; i++)
		{
			ray[i] = surface->eye[i];
			ray[3 + i] = surface->center[i] + (2.0f * menu_script.x0 - 1.0f) * surface->x_axis[i] +
				(1.0f - 2.0f * menu_script.y0) * surface->y_axis[i] - ray[i];
		}
		length = sqrtf(host_menu_gaze_dot(ray + 3, ray + 3));
		t = menu_script.kind == 7 && menu_script.step > 0 ?
			fminf(1.0f, (float)menu_script.step / (float)(last - 2)) : 0.0f;
		for (int i = 0; i < 3; i++)
			hand[i] = ray[i] + (i == 1 ? -0.3f : 0.0f) + (length > 0.0f ? 0.3f * ray[3 + i] / length : 0.0f) +
				t * 0.01f * (menu_script.x1 * surface->x_axis[i] / width - menu_script.y1 * surface->y_axis[i] / height);
	}
	host_menu_gaze_spatial_event(pinch, phase, 1, ray, 1, hand);
	menu_script.step++;
	if (menu_script.step > last)
		menu_script_next();
	return 1;
}

static void menu_script_run(void)
{
	SDL_Window *window = host_sdl_window();
	int width = 0, height = 0;
	float x, y, t;

	if (!menu_script.loaded)
	{
		menu_script.loaded = 1;
		menu_script.cursor = getenv("HALO_MENU_POINTER_SCRIPT");
		menu_script_next();
	}
	menu_script.frame++;
	if (!menu_script.kind || !window || !SDL_GetWindowSize(window, &width, &height) ||
		menu_script.frame < menu_script.start + (unsigned long)menu_script.step)
		return;
	if (menu_script.kind >= 6 && menu_script_pinch())
		return;
	host_menu_pointer_set_surface(&menu_pointer, (float)width, (float)height);
	t = menu_script.steps > 1 ? (float)menu_script.step / (float)(menu_script.steps - 1) : 0.0f;
	x = (menu_script.x0 + (menu_script.x1 - menu_script.x0) * t) * (float)width;
	y = (menu_script.y0 + (menu_script.y1 - menu_script.y0) * t) * (float)height;
	switch (menu_script.kind)
	{
	case 1:
	case 2:
		if (menu_script.step == 0)
			host_menu_pointer_touch_begin(&menu_pointer, 9000, x, y);
		else if (menu_script.step < menu_script.steps)
			host_menu_pointer_touch_move(&menu_pointer, 9000, x, y);
		if (menu_script.kind == 1 && menu_script.step == 1)
			host_menu_pointer_touch_end(&menu_pointer, 9000);
		if (menu_script.kind == 2 && menu_script.step == menu_script.steps)
			host_menu_pointer_touch_end(&menu_pointer, 9000);
		menu_script.step++;
		if (menu_script.kind == 1 ? menu_script.step > 1 : menu_script.step > menu_script.steps)
			menu_script_next();
		return;
	case 3:
	case 4:
		host_menu_pointer_device_move(&menu_pointer, x, y);
		host_menu_pointer_device_click(&menu_pointer, menu_script.kind == 4, x, y);
		break;
	default:
		host_menu_pointer_device_wheel(&menu_pointer, (int)menu_script.x0);
		break;
	}
	menu_script_next();
}

void host_ui_pointer_read(struct host_ui_pointer *pointer)
{
	memset(pointer, 0, sizeof(*pointer));
	menu_pointer_sync();
#if !TARGET_OS_TV
	menu_script_run();
	host_menu_pointer_read(&menu_pointer, pointer);
#endif
}

void host_ui_pointer_feedback(int kind)
{
	if (pointer_feedback)
		pointer_feedback(kind);
}

int host_sdl_text_result(char *buffer, uint32_t size)
{
#if TARGET_OS_TV
	return host_tv_text_result(buffer, size);
#else
	(void)buffer;
	(void)size;
	return 0;
#endif
}

static void text_input_update(void)
{
	SDL_Window *window;

#if TARGET_OS_TV
	host_tv_text_prompt_tick();
#endif
	if (text_input_wanted == text_input_on || !(window = host_sdl_window()))
		return;
#if TARGET_OS_TV
	if (host_tv_text_prompt(text_input_wanted, text_input_title, text_input_text, text_input_masked))
		text_input_on = text_input_wanted;
#else
	text_input_on = text_input_wanted;
	if (text_input_on)
	{
		SDL_StartTextInput(window);
		host_logf(HOST_LOG_INFO, "keyboard: up for a text field");
	}
	else
	{
		SDL_StopTextInput(window);
	}
#endif
}

/* the guest's window (host_sdl_create_window makes one), for the GPU backend's
present in the host (gpu_gl.c, host_gpu.c) */
SDL_Window *host_sdl_window(void)
{
	unsigned index;

	for (index = 1; index < HANDLE_COUNT; index++)
		if (handles[index].type == _handle_window)
			return handles[index].object;
	return NULL;
}

/* UIKit renders to a view framebuffer, rather than framebuffer zero. */
uint32_t host_ios_default_framebuffer(void)
{
	SDL_Window *window = host_sdl_window();

	return window ? (uint32_t)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
		SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0) : 0;
}

/* ---------- messages for the player (sdl_platform.c, guest_sdl.c) */

/* Android's short toast; iOS has none, so the message isn't shown */
int host_sdl_show_toast(const char *message, int duration, int gravity, int x, int y)
{
	(void)message;
	(void)duration;
	(void)gravity;
	(void)x;
	(void)y;
	return 0;
}

int host_sdl_show_simple_message_box(unsigned int flags, const char *title, const char *message)
{
	if (host_alert(title, message, "OK", NULL) >= 0)
		return 1;
	return SDL_ShowSimpleMessageBox((SDL_MessageBoxFlags)flags, title, message, host_sdl_window());
}

int host_sdl_show_message_box(unsigned int flags, const char *title, const char *message, const char *first,
	const char *second)
{
	int shown = host_alert(title, message, first, second);
	SDL_MessageBoxButtonData buttons[2] = {
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, first },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, second },
	};
	SDL_MessageBoxData data = { (SDL_MessageBoxFlags)flags, host_sdl_window(), title, message, second ? 2 : 1,
		buttons, NULL };
	int answer = -1;

	if (shown >= 0)
		return shown;
	return SDL_ShowMessageBox(&data, &answer) ? answer : -1;
}

/* Settings use the same preference as Settings.bundle on every Apple host. */
int host_sdl_cloud_saves_enabled(void)
{
    return host_cloud_saves_enabled();
}

void host_sdl_cloud_saves_set_enabled(int enabled)
{
    host_cloud_saves_set_enabled(enabled);
}

void host_sdl_cloud_saves_status(char *buffer, unsigned int size)
{
    if (!size) return;
    strncpy(buffer, host_cloud_saves_status(), size - 1);
    buffer[size - 1] = 0;
}

void host_sdl_app_version(char *buffer, unsigned int size)
{
    CFTypeRef version = host_app_version_string(CFBundleGetMainBundle());

    if (!size) return;
    buffer[0] = 0;
    if (version && CFGetTypeID(version) == CFStringGetTypeID())
        CFStringGetCString((CFStringRef)version, buffer, (CFIndex)size, kCFStringEncodingUTF8);
    buffer[size - 1] = 0;
}
