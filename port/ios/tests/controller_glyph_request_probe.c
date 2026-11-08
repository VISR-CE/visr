#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
typedef int BOOL;
typedef unsigned char BYTE;
typedef char CHAR;
#include "sdl_platform.h"
#include "halo_host_pointer.h"
typedef struct {unsigned short wButtons;unsigned char bAnalogButtons[8];short sThumbLX,sThumbLY,sThumbRX,sThumbRY;} XINPUT_GAMEPAD;
#include "halo_controller_glyphs.h"
#include "halo_keyboard.h"
#define PORT_COUNT 4
#define MAXIMUM_BINDINGS 2
#define HALO_IOS 1

static int dev, kind = 2, lookup_player = -1, split, keyboard_active, last_menus;
static int platform_menu_keyboard(void) { return keyboard_active; }
static int glyph_keyboard_active;
static int glyph_menus_active;
static struct halo_glyph_input glyph_menu_input;
static int bindings[17][2];
static unsigned long bindings_read_at=(unsigned long)-1, generation;
static const char *configured="Q";
/* BINDING SETTINGS */
static const char *override;
static int host_development_hooks(void) { return dev; }
static const char *config_string(const char *name) { return !strcmp(name,"debug.controller_glyph_family") ? (override?override:"") : !strcmp(name,"controls.scoreboard") ? "T" : !strcmp(name,"controls.pause") ? "P" : configured; }
static unsigned long config_changes(void) {return generation;}
int halo_input_from_name(const char *name) {return strlen(name)==1?name[0]:-1;}
#define platform_log(...) ((void)0)
static int sdl_gamepads(SDL_Gamepad **pads) { pads[0]=(void *)(uintptr_t)21;pads[1]=(void *)(uintptr_t)22;return 2; }
static SDL_Gamepad *port_gamepad(SDL_Gamepad **pads, int count, int port) {
 (void)count;lookup_player=port;return split ? (port==1?pads[0]:NULL) : pads[port];
}
static int host_sdl_gamepad_glyph(unsigned int pad,int physical,const char *family,char *label,unsigned int size, int player, int menus) {
 (void)physical; (void)player; last_menus=menus;
 if (!strcmp(family,"keyboard")) return 3;
 if (!strcmp(family,"touch")) return 1;
 snprintf(label,size,"pad.%u",pad);return kind;
}
static SDL_Gamepad *probe_gamepad_from_id(unsigned int id) {return id==22?(void *)(uintptr_t)22:NULL;}
#define SDL_GetGamepadFromID probe_gamepad_from_id
/* BINDINGS READ */
void halo_input_name(int input,char *label,size_t size) { snprintf(label,size,"%c",input); }
/* ACTIVITY */
/* REQUEST */
int main(void) {
 char label[96];
 struct host_ui_pointer pointer={0};
 struct platform_input_state input={0};
 XINPUT_GAMEPAD pad={0};
 assert(halo_controller_glyph_request(-1,18,2,label,sizeof(label))==0);
 assert(lookup_player==-1);
 assert(halo_controller_glyph_request(4,18,2,label,sizeof(label))==0 && lookup_player==-1);
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==2 && !strcmp(label,"pad.21"));
 assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==2 && !strcmp(label,"pad.22"));
 split=1;assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==2 && !strcmp(label,"pad.21"));split=0;
 kind=3;assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==3 && !strcmp(label,"Q"));
 configured="F";generation++;
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==3 && !strcmp(label,"F"));
 configured="Unbound,Q";generation++;
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==3 && !strcmp(label,"Q"));
 assert(halo_controller_glyph_request(0,-1,0,label,sizeof(label))==3 && !strcmp(label,"Enter"));
 assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==0);
 kind=1;assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==1);
 kind=0;assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==0);
 kind=2;override="touch,keyboard";
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==2); /* shipping ignores override */
 dev=1;assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==1);
 assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==3 && !strcmp(label,"Q"));
 override="";glyph_keyboard_active=1;keyboard_active=1;
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==3 && !strcmp(label,"Q"));
 assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==2);
 glyph_keyboard_active=0;assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==2);
 pointer.active=1;pointer.radius=22;
 halo_controller_glyph_pointer_activity(1,&pointer);assert(glyph_keyboard_active==0);
 pointer.moved=1;halo_controller_glyph_pointer_activity(1,&pointer);
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==1);
 pointer.radius=0;pointer.picture=1;
 SDL_Event event={.type=SDL_EVENT_MOUSE_MOTION};event.motion.xrel=4;
 halo_controller_glyph_input_event(&event);
 halo_controller_glyph_pointer_activity(1,&pointer);
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==3 && last_menus==1);
 assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==3);
 halo_controller_glyph_pointer_activity(0,NULL);
 assert(halo_controller_glyph_request(0,13,13,label,sizeof(label))==3 && !strcmp(label,"T"));
 assert(halo_controller_glyph_request(0,12,12,label,sizeof(label))==3 && !strcmp(label,"P"));
 halo_controller_glyph_pointer_activity(1,NULL);
 assert(halo_controller_glyph_request(0,13,13,label,sizeof(label))==3 && !strcmp(label,"F1"));
 assert(halo_controller_glyph_request(0,28,5,label,sizeof(label))==3 && !strcmp(label,"Enter"));
 glyph_keyboard_active=0;input.focused=1;input.keys[1]=1;keyboard_active=0;
 glyph_keyboard_activity(&input);assert(glyph_keyboard_active==0); /* software keyboard */
 keyboard_active=1;glyph_keyboard_activity(&input);assert(glyph_keyboard_active==1);
 memset(&input,0,sizeof(input));input.focused=1;input.mouse_dx=4;
 glyph_keyboard_active=0;glyph_keyboard_activity(&input);assert(glyph_keyboard_active==1);
 assert(!glyph_gamepad_active(&pad));pad.sThumbRX=10000;assert(glyph_gamepad_active(&pad));
 glyph_keyboard_active=glyph_gamepad_active(&pad)?0:1;
 event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=22;
 halo_controller_glyph_input_event(&event);
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==2);
 /* Pure policy: controller -> keyboard -> touch -> a different controller.
    Idle samples preserve both the family and the most recent pad identity. */
 struct halo_glyph_input selected={0};
 selected=halo_glyph_input_note(selected,0,21,1);
 assert(selected.source==0 && selected.gamepad==21);
 selected=halo_glyph_input_note(selected,1,0,1);
 assert(selected.source==1 && selected.gamepad==0);
 selected=halo_glyph_input_note(selected,0,21,0);
 assert(selected.source==1);
 selected=halo_glyph_input_note(selected,-2,0,1);
 assert(selected.source==-2);
 selected=halo_glyph_input_note(selected,0,22,1);
 assert(selected.source==0 && selected.gamepad==22);
 /* Events select the last controller for every menu legend owner. */
 event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.which=22;
 halo_controller_glyph_input_event(&event);
 assert(halo_controller_glyph_request(0,-1,2,label,sizeof(label))==2 && !strcmp(label,"pad.22"));
 event.type=SDL_EVENT_KEY_DOWN;event.key.repeat=0;
 halo_controller_glyph_input_event(&event);
 for(int face=0;face<4;face++) {
  assert(halo_controller_glyph_request(0,-1,face,label,sizeof(label))==3);
  assert(!strcmp(label,halo_glyph_menu_key(face)));
 }
 event.type=SDL_EVENT_FINGER_DOWN;halo_controller_glyph_input_event(&event);
 assert(halo_controller_glyph_request(0,-1,2,label,sizeof(label))==1);
 event.type=SDL_EVENT_KEY_DOWN;event.key.repeat=1;
 halo_controller_glyph_input_event(&event);
 assert(halo_controller_glyph_request(0,-1,2,label,sizeof(label))==1);
 event.key.repeat=0;halo_controller_glyph_input_event(&event);
 assert(halo_controller_glyph_request(0,-1,2,label,sizeof(label))==3);
 return 0;
}
