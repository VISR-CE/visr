#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
typedef void SDL_Gamepad;
static int dev, kind = 2, lookup_player = -1, split, keyboard_active, last_menus;
static int platform_menu_keyboard(void) { return keyboard_active; }
static int glyph_keyboard_active;
static int glyph_menus_active;
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
 pointer.radius=0;pointer.picture=1;halo_controller_glyph_pointer_activity(1,&pointer);
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==3 && last_menus==1);
 assert(halo_controller_glyph_request(1,18,2,label,sizeof(label))==2);
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
 assert(halo_controller_glyph_request(0,18,2,label,sizeof(label))==2);
 return 0;
}
