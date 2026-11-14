/* SDL window calls must use main; cached reads must not dispatch per frame. */
#include <SDL3/SDL.h>
#include "host_game_thread.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TARGET_OS_VISION 0
#undef TARGET_OS_MACCATALYST
#define TARGET_OS_MACCATALYST PROBE_MACCATALYST
#undef TARGET_OS_IOS
#define TARGET_OS_IOS 1
#define HALO_NATIVE_MAC PROBE_NATIVE_MAC
#define HOST_LOG_INFO 2
#define HOST_LOG_WARN 3
#define HOST_LOG_ERROR 1
static int on_main = 1, dispatches, sequence, fail_create, fail_size, fail_view;
static const char *configured;
static char reported_error[256];
static int app_on_mac = 1, relative_result = 1, pixel_reads, point_reads;
static int live_width = 640, live_height = 480;
static int framebuffer = 17;
static SDL_Window *test_window = (SDL_Window *)(uintptr_t)0x1000;
static SDL_MetalView test_view = (SDL_MetalView)(uintptr_t)0x2000;
void host_config_string(const char *name, const char *fallback, char *out, size_t size) {
    if (!strcmp(name, "debug.game_thread")) {
        assert(!strcmp(fallback, "true"));
        snprintf(out, size, "%s", configured);
    } else {
        assert(!strcmp(name, "display.mode") || !strcmp(name, "display.fullscreen"));
        snprintf(out, size, "%s", fallback);
    }
}
void SDL_PumpEvents(void) { assert(on_main); live_width++; live_height++; }
int host_on_main_thread(void) { return on_main; }
void host_main_sync_f(void *context, void (*fn)(void *)) {
    int previous = on_main;
    if (!on_main) dispatches++;
    on_main = 1;
    fn(context);
    on_main = previous;
}
void host_logf(int level, const char *format, ...) { (void)level; (void)format; }
int host_ios_app_on_mac(void) { return app_on_mac; }
SDL_Window *SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags) {
    assert(on_main && sequence++ == 0);
    assert(!strcmp(title, HALO_NATIVE_MAC ? "VISR" : "probe") && w == 640 && h == 480);
    (void)flags;
    return fail_create ? NULL : test_window;
}
SDL_MetalView SDL_Metal_CreateView(SDL_Window *window) {
    assert(on_main && window == test_window && sequence++ == 1);
    return fail_view ? NULL : test_view;
}
void host_ios_touch_attach(SDL_Window *window) {
    assert(on_main && window == test_window && sequence++ == 2);
}
void host_external_attach(void *window, int metal) {
    assert(on_main && window == test_window && metal && sequence++ == 3);
}
void *SDL_Metal_GetLayer(SDL_MetalView view) { assert(on_main && view == test_view); return view; }
bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h) {
    assert(on_main && window == test_window);
    pixel_reads++;
    if (fail_size) return false;
    *w = live_width * 2; *h = live_height * 2; return true;
}
bool SDL_GetWindowSize(SDL_Window *window, int *w, int *h) {
    assert(on_main && window == test_window);
    point_reads++;
    if (fail_size) return false;
    *w = live_width; *h = live_height; return true;
}
SDL_WindowID SDL_GetWindowID(SDL_Window *window) { assert(on_main && window == test_window); return 42; }
SDL_Window *SDL_GetWindowFromID(SDL_WindowID id) { assert(on_main); return id == 42 ? test_window : NULL; }
SDL_PropertiesID SDL_GetWindowProperties(SDL_Window *window) {
    assert(on_main && window == test_window); return 77;
}
Sint64 SDL_GetNumberProperty(SDL_PropertiesID properties, const char *name, Sint64 fallback) {
    assert(properties == 77 && !strcmp(name, SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER));
    (void)fallback; return framebuffer;
}
bool SDL_SetWindowRelativeMouseMode(SDL_Window *window, bool enabled) {
    assert(on_main && window == test_window); (void)enabled; return relative_result;
}
bool SDL_SetError(const char *format, ...) {
    assert(!strcmp(format, "%s"));
    va_list arguments; va_start(arguments, format);
    snprintf(reported_error, sizeof(reported_error), "%s", va_arg(arguments, const char *));
    va_end(arguments); return false;
}
const char *SDL_GetError(void) { return "probe error"; }
int host_external_size(int *w, int *h) { (void)w; (void)h; return 0; }
/* tools/ios_test.py extracts the real handle table and window helpers. */
#include "game_thread_window.inc"

int main(int argc, char **argv) {
    assert(argc == 2);
    configured = argv[1];
    assert(host_game_thread_enabled() == !strcmp(configured, "true"));
    for (int main_caller = 0; main_caller <= 1; main_caller++) {
        on_main = main_caller; sequence = dispatches = 0;
        uint32_t handle = host_sdl_create_window("probe", 640, 480, SDL_WINDOW_METAL);
        assert(handle && sequence == 4 && metal_view == test_view);
        assert(dispatches == !main_caller);
        assert(host_sdl_metal_layer() == test_view);
        assert(host_sdl_set_relative_mouse(handle, 1) == 1);
        relative_result = 0;
        assert(host_sdl_set_relative_mouse(handle, 0) == 0);
        relative_result = 1;
        int w = -1, h = -1, before = dispatches;
        host_sdl_window_size_in_pixels(handle, &w, &h);
        assert(w == live_width * 2 && h == live_height * 2);
        assert(window_size(test_window, &w, &h) && w == live_width && h == live_height);
        assert(window_from_id(42) == test_window && !window_from_id(43));
        assert(host_ios_default_framebuffer() == (uint32_t)framebuffer);
        framebuffer++;
        assert(host_ios_default_framebuffer() == (uint32_t)framebuffer);
        assert(dispatches == before);
        on_main = 1;
        host_event_pump();
        on_main = main_caller;
        host_sdl_window_size_in_pixels(handle, &w, &h);
        assert(w == live_width * 2 && h == live_height * 2);
        if (!main_caller) {
            int pixels = pixel_reads, points = point_reads;
            for (int i = 0; i < 120; i++) {
                host_sdl_window_size_in_pixels(handle, &w, &h);
                assert(window_size(test_window, &w, &h));
            }
            assert(pixel_reads == pixels && point_reads == points && dispatches == before);
        }
        host_sdl_window_size_in_pixels(0, &w, &h);
        assert(!w && !h && !host_sdl_set_relative_mouse(0, 1));
        on_main = 1; fail_size = 1; host_sdl_refresh_window_cache();
        on_main = main_caller;
        assert(!window_size(test_window, &w, &h));
        fail_size = 0;
        fail_create = 1; sequence = 0;
        assert(!host_sdl_create_window("probe", 640, 480, SDL_WINDOW_METAL));
        assert(sequence == 1);
        if (!main_caller) assert(!strcmp(reported_error, "probe error")); fail_create = 0;
        sequence = 0;
        assert(host_sdl_create_window("probe", 640, 480, SDL_WINDOW_OPENGL));
        assert(sequence == 1);
        sequence = 0; fail_view = 1;
        assert(host_sdl_create_window("probe", 640, 480, SDL_WINDOW_METAL));
        assert(sequence == 4 && !metal_view); fail_view = 0;
    }
#if !TARGET_OS_MACCATALYST && !HALO_NATIVE_MAC
    on_main = 0; app_on_mac = 0; dispatches = 0;
    assert(host_sdl_set_relative_mouse(1, 1) == 1 && !dispatches);
#endif
    puts("PASS: main-thread window creation and mouse mode; live main reads and cached worker reads");
    return 0;
}
