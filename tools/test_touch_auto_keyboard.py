"""Exercise touch keyboard eligibility and form completion without a window."""
from pathlib import Path
import shutil
import subprocess
from probekit import source as probe_source

ROOT = Path(__file__).resolve().parents[1]




def run_probe(tmp_path, source):
    program = tmp_path / "keyboard.c"
    binary = tmp_path / "keyboard"
    program.write_text(source)
    subprocess.run([shutil.which("cc"), "-std=c11", "-D_DEFAULT_SOURCE", "-Wall", "-Werror",
                    "--include-directory=" + str(ROOT / "port/ios/host"),
                    "--include-directory=" + str(ROOT / "port/runtime/include"),
                    str(program), "--output=" + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


def test_auto_keyboard_requires_direct_touch_and_no_hardware_keyboard(tmp_path):
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stdint.h>
#include "host_menu_pointer.h"
int main(void) {
    struct host_menu_pointer state = {0};
    for (int source = 0; source <= 2; source++) {
        state.touch = source;
        for (int keyboard = 0; keyboard <= 1; keyboard++)
            assert(host_menu_pointer_auto_keyboard(&state, keyboard) == (source == 1 && !keyboard));
    }
    return 0;
}
''')


def test_finish_retains_text_and_focuses_action_or_next_field(tmp_path):
    source = "port/linux/game/menu_functions.c"
    run_probe(tmp_path, r'''
#include <assert.h>
#include <string.h>
#include <stddef.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
struct widget_instance {
    const char *name;
    int type, visible;
    struct widget_instance *parent, *child, *next, *focused_child;
    struct { struct { short selected_index; } list; } parameters;
};
static struct widget_instance screen, list, field, next, action, bar;
static struct { struct widget_instance *row; } text_field;
static int kept;
static struct widget_instance *screen_of(struct widget_instance *row) { (void)row; return &screen; }
static struct widget_instance *named(struct widget_instance *root, const char *name, int nth) {
    (void)root; (void)nth;
    if (!strcmp(action.name, name)) return &action;
    if (!strcmp(next.name, name)) return &next;
    return NULL;
}
static void text_field_end(boolean keep) { kept = keep; text_field.row = NULL; }
''' + probe_source.function(source, "text_field_focus") +
        probe_source.function(source, "text_field_finish") + r'''
int main(void) {
    screen.child = &list; list.parent = &screen; list.type = 3;
    list.child = &field; field.parent = &list; field.next = &next;
    next.parent = &list; next.next = &action; next.visible = 1;
    action.parent = &list; action.name = "button_ok";
    field.name = "op_code"; next.name = "unused";
    text_field.row = &field; text_field_finish(FALSE);
    assert(kept && !text_field.row && list.focused_child == &action);
    assert(list.parameters.list.selected_index == 2 && screen.focused_child == &list);
    action.name = "op_start_game"; field.name = "op_server_name"; next.name = "op_password";
    text_field.row = &field; text_field_finish(TRUE);
    assert(list.focused_child == &next && list.parameters.list.selected_index == 1);
    next.visible = 0;
    text_field.row = &field; text_field_finish(TRUE);
    assert(list.focused_child == &action);
    action.name = "direct_ip_button_join"; field.name = "op_server_address"; next.name = "op_server_port";
    next.visible = 1; text_field.row = &field; text_field_finish(TRUE);
    assert(list.focused_child == &next);
    text_field.row = &next; text_field_finish(TRUE);
    assert(list.focused_child == &action);
    /* The PC menu's action is nested inside its button bar. */
    action.parent = &bar; bar.parent = &list; bar.child = &action;
    next.next = &bar; bar.type = 3; bar.next = NULL;
    text_field.row = &field; text_field_finish(FALSE);
    assert(list.focused_child == &bar && bar.focused_child == &action);
    assert(list.parameters.list.selected_index == 2 && bar.parameters.list.selected_index == 0);
    return 0;
}
''')


def test_name_prompt_commits_through_existing_validation_and_cancels(tmp_path):
    source = "source/interface/virtual_keyboard.c"
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#define MAXIMUM_VIRTUAL_KEYBOARD_BUFFER_SIZE 64
#define FALSE 0
static int virtual_keyboard_system;
static struct { int active, buffer_size, row, column; wchar_t *text_buffer; } virtual_keyboard_globals;
static int response, selected, canceled, processed, closed;
void platform_text_typing(int typing) { (void)typing; }
int platform_text_field_result(char *text, int size) { (void)size; strcpy(text, "Typed"); return response; }
void platform_text_field(int on, const char *title, const char *text, int masked) {
    (void)title; (void)text; (void)masked; assert(!on); closed++;
}
static void virtual_keyboard_cancel(void) { canceled++; virtual_keyboard_globals.active = 0; }
static void virtual_keyboard_select(void) {
    assert(!virtual_keyboard_globals.row && !virtual_keyboard_globals.column);
    selected++; virtual_keyboard_globals.active = 0;
}
static void virtual_keyboard_process_internal(void) { processed++; }
''' + probe_source.function(source, "virtual_keyboard_process") + r'''
int main(void) {
    wchar_t buffer[4] = {0};
    virtual_keyboard_globals.text_buffer = buffer;
    virtual_keyboard_globals.buffer_size = sizeof(buffer);
    for (response = 0; response <= 4; response++) {
        if (response == 1) continue; /* tvOS uses its existing path. */
        selected = canceled = processed = closed = 0;
        virtual_keyboard_system = virtual_keyboard_globals.active = 1;
        virtual_keyboard_globals.row = virtual_keyboard_globals.column = 3;
        virtual_keyboard_process();
        assert(!processed);
        assert(canceled == (response == 2));
        assert(selected == (response == 3 || response == 4));
        if (selected) {
            assert(buffer[0] == L'T' && buffer[2] == L'p' && !buffer[3]);
            assert(closed == 1 && !virtual_keyboard_system);
        }
    }
    virtual_keyboard_system = 0; virtual_keyboard_globals.active = 1;
    virtual_keyboard_process(); assert(processed == 1);
    return 0;
}
''')


def test_auto_open_keeps_tv_and_hardware_entry(tmp_path):
    source = "port/runtime/guest/runtime/guest_sdl.c"
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
static int touch, keyboard, tv;
static const char *fake_env(const char *name) { (void)name; return tv ? "1" : NULL; }
#define getenv fake_env
int host_menu_touch(void) { return touch; }
int host_menu_keyboard(void) { return keyboard; }
''' + probe_source.function(source, "platform_text_auto_open") + r'''
int main(void) {
    for (tv = 0; tv <= 1; tv++)
        for (touch = 0; touch <= 1; touch++)
            for (keyboard = 0; keyboard <= 1; keyboard++)
                assert(platform_text_auto_open() == (tv || touch || keyboard));
    return 0;
}
''')


def test_host_retries_presentation_and_restarts_between_fields(tmp_path):
    update = probe_source.function("port/ios/host/host_sdl.c", "text_input_update")
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stddef.h>
#define TARGET_OS_TV 0
#define TARGET_OS_VISION 0
#define HALO_NATIVE_MAC 0
#define HOST_LOG_INFO 0
typedef int SDL_Window;
static int text_input_wanted, text_input_on, text_input_masked, text_input_touch;
static unsigned int text_input_revision, text_input_applied;
static char text_input_title[64], text_input_text[512];
static int starts, stops, prompts, dismissals, ready, touch = 1;
static SDL_Window window;
static SDL_Window *host_sdl_window(void) { return &window; }
static void SDL_StartTextInput(SDL_Window *w) { (void)w; starts++; }
static void SDL_StopTextInput(SDL_Window *w) { (void)w; stops++; }
static void text_input_start_main(void *w) { SDL_StartTextInput((SDL_Window *)w); }
static void text_input_stop_main(void *w) { SDL_StopTextInput((SDL_Window *)w); }
static void host_main_sync_f(void *context, void (*fn)(void *)) { fn(context); }
static void host_logf(int level, const char *text) { (void)level; (void)text; }
static int host_menu_touch(void) { return touch; }
static int host_menu_text_prompt(int on, const char *title, const char *text, int masked) {
    (void)title; (void)text; (void)masked;
    if (on) prompts++; else dismissals++;
    return !on || ready;
}
''' + update + r'''
int main(void) {
    text_input_wanted = 1; text_input_revision++;
    text_input_update(); assert(prompts == 1 && !text_input_on);
    ready = 1; text_input_update(); assert(prompts == 2 && text_input_on);
    text_input_update(); assert(prompts == 2);
    /* End and begin can both happen before the next host poll. */
    text_input_revision++; text_input_update();
    assert(dismissals == 1 && prompts == 3 && text_input_on);
    text_input_wanted = 0; text_input_update();
    assert(dismissals == 2 && !text_input_on && !starts && !stops);
    touch = 0; text_input_wanted = 1; text_input_revision++;
    text_input_update(); assert(starts == 1 && text_input_on);
    text_input_wanted = 0; text_input_update(); assert(stops == 1);
    return 0;
}
''')


def test_touch_return_submits_once_after_field_closes(tmp_path):
    source = "port/linux/src/xinput_sdl.c"
    run_probe(tmp_path, r'''
#include <assert.h>
#include <stddef.h>
#define HALO_IOS 1
#define FALSE 0
#define TRUE 1
typedef int BOOL;
typedef unsigned char BYTE;
static int text_typing_field, text_typing_submit, text_typing_enter_armed;
static int host_calls;
static void halo_host_text_input(int on, const char *title, const char *text, int masked) {
    (void)on; (void)title; (void)text; (void)masked; host_calls++;
}
static void text_typing_update(void) {}
static BYTE analog(BOOL down) { return down ? 255 : 0; }
enum { SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
       SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_ESCAPE };
enum { XINPUT_GAMEPAD_START=1, XINPUT_GAMEPAD_DPAD_UP=2, XINPUT_GAMEPAD_DPAD_DOWN=4,
       XINPUT_GAMEPAD_DPAD_LEFT=8, XINPUT_GAMEPAD_DPAD_RIGHT=16, XINPUT_GAMEPAD_B=0 };
struct platform_input_state { unsigned char keys[7]; };
typedef struct { unsigned int wButtons; unsigned char bAnalogButtons[1]; } XINPUT_GAMEPAD;
''' + probe_source.function(source, "platform_text_field") +
        probe_source.function(source, "platform_text_field_submit") +
        probe_source.function(source, "typing_gamepad") + r'''
int main(void) {
    struct platform_input_state input = {0};
    XINPUT_GAMEPAD pad = {0};
    platform_text_field(1, "Name", "", 0);
    platform_text_field(0, NULL, NULL, 0);
    platform_text_field_submit();
    assert(!text_typing_field && text_typing_submit);
    typing_gamepad(&input, &pad);
    assert(pad.wButtons == XINPUT_GAMEPAD_START && !text_typing_submit);
    pad.wButtons = 0; typing_gamepad(&input, &pad); assert(!pad.wButtons);
    assert(host_calls == 2);
    return 0;
}
''')
