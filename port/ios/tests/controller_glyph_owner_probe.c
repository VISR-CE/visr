#include <assert.h>
#include <stddef.h>
#include <wchar.h>
typedef int boolean;
typedef unsigned short word;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
#define FIRST_VIRTUAL_KEYBOARD_CAPTION_STRING_INDEX 0
#define NUMBER_OF_VIRTUAL_KEYBOARD_STRINGS 12
#define MAXIMUM_VIRTUAL_KEYBOARD_BUFFER_SIZE 64
#define MAXIMUM_VIRTUAL_KEYBOARD_SAVED_TEXT_LENGTH 12
#define _ui_audio_feedback_forward 0
#define match_assert(file,line,condition) assert(condition)
#define ustrlen wcslen
#define ustrncpy wcsncpy
static short virtual_keyboard_glyph_player,virtual_keyboard_glyph_context;
static struct {int active;void *keyboard;int row,column;wchar_t *text_buffer,*cursor;word buffer_size;int last_event,time_of_last_event,caption_index,shift_active,caps_active,symbols_active,first_key_replaces_buffer,last_exit_saved_text;wchar_t saved_text[12];} virtual_keyboard_globals;
static int event_manager_flush(void) {return 0;}
static int system_milliseconds(void) {return 1;}
static void ui_play_audio_feedback_sound(int sound) {(void)sound;}
/* OWNER */
/* LAUNCH */
int main(void) {
 wchar_t text[16]=L"Player";virtual_keyboard_globals.keyboard=&text;
 virtual_keyboard_set_controller_context(2);
 assert(virtual_keyboard_launch(text,sizeof(text),8));
 assert(virtual_keyboard_glyph_player==2); /* first draw, no subsequent input */
 virtual_keyboard_set_controller_context(0);assert(virtual_keyboard_glyph_player==2);
 virtual_keyboard_globals.active=0;virtual_keyboard_set_controller_context(1);
 virtual_keyboard_set_controller_context(-1);virtual_keyboard_set_controller_context(4);
 assert(virtual_keyboard_launch(text,sizeof(text),8) && virtual_keyboard_glyph_player==1);
 return 0;
}
