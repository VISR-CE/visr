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
#include "halo_pointer_gate.h"
#include "host_menu_pointer.h"
#include "halo_keyboard.h"
#define PORT_COUNT 4
#define MAXIMUM_BINDINGS 2
#define HALO_IOS 1

static int dev, kind = 2, lookup_player = -1, split, keyboard_active, last_menus;
static int platform_menu_keyboard(void) { return keyboard_active; }
static int glyph_keyboard_active;
static int glyph_mouse_active;
static int glyph_menus_active;
static int glyph_x_destructive;
static struct halo_glyph_input glyph_menu_input;
static int bindings[17][2];
static unsigned long bindings_read_at=(unsigned long)-1, generation;
static const char *configured="Q";
/* BINDING SETTINGS */
static const char *override;
static int host_development_hooks(void) { return dev; }
static const char *config_string(const char *name) { return !strcmp(name,"debug.controller_glyph_family") ? (override?override:""):""; }
/* the profile of the player on gamepad 0: its bindings are the names in `configured` (Q or "Unbound,Q": a name of one
letter is a key, any other is none), the scoreboard's T and the pause's P */
unsigned short player_input_profile_binding(short player, short action, short slot) {
 const char *text=action==15?"T":action==16?"P":configured;
 char name[32]="", token[32]; size_t length; int index; (void)player;
 for(index=0;index<=slot;index++){ while(*text==' '||*text==',')text++; length=strcspn(text,","); snprintf(token,sizeof(token),"%.*s",(int)length,text); text+=length; if(index==slot)snprintf(name,sizeof(name),"%s",token); }
 return strlen(name)==1?(unsigned short)name[0]:0;
}
static int input_from_stable_code(unsigned short code) { return code?(int)code:-1; }
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
 assert(halo_controller_glyph_request(0,28,5,label,sizeof(label))==3 && !strcmp(label,"Mouse Left"));
 assert(halo_controller_glyph_request(0,2,2,label,sizeof(label))==3 && !strcmp(label,"Mouse Right"));
 halo_controller_glyph_pointer_activity(0,NULL);
 assert(halo_controller_glyph_request(0,13,13,label,sizeof(label))==3 && !strcmp(label,"T"));
 assert(halo_controller_glyph_request(0,12,12,label,sizeof(label))==3 && !strcmp(label,"P"));
 halo_controller_glyph_pointer_activity(1,NULL);
 assert(halo_controller_glyph_request(0,13,13,label,sizeof(label))==3 && !strcmp(label,"F1"));
 assert(halo_controller_glyph_request(0,28,5,label,sizeof(label))==3 && !strcmp(label,"Mouse Left"));
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
 /* The mouse is separate from the keyboard; small motion never claims the prompts. */
 selected=halo_glyph_input_note(selected,HALO_GLYPH_INPUT_KEYBOARD,0,1);
 selected=halo_glyph_input_note(selected,HALO_GLYPH_INPUT_MOUSE,0,halo_glyph_mouse_motion_active(1.0f,1.0f));
 assert(selected.source==HALO_GLYPH_INPUT_KEYBOARD);
 selected=halo_glyph_input_note(selected,HALO_GLYPH_INPUT_MOUSE,0,halo_glyph_mouse_motion_active(2.0f,0.0f));
 assert(selected.source==HALO_GLYPH_INPUT_MOUSE && selected.gamepad==0);
 selected=halo_glyph_input_note(selected,HALO_GLYPH_INPUT_KEYBOARD,0,1);
 assert(selected.source==HALO_GLYPH_INPUT_KEYBOARD);
 selected=halo_glyph_input_note(selected,HALO_GLYPH_INPUT_MOUSE,0,1); /* a click */
 assert(selected.source==HALO_GLYPH_INPUT_MOUSE);
 selected=halo_glyph_input_note(selected,0,22,1);
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
 /* Reactivation (Cmd-Tab back): the host classifies each event once; the legend and the menu pointer read it. */
 {
  struct halo_pointer_gate gate;
  struct host_menu_pointer pointer;
  SDL_Event e, motion;
  struct host_ui_pointer out;
  memset(&gate,0,sizeof(gate));memset(&pointer,0,sizeof(pointer));
  host_menu_pointer_set_surface(&pointer,874,402);host_menu_pointer_set_menus(&pointer,1);
  memset(&motion,0,sizeof(motion));
#define FEED(ev) do { int real_=halo_pointer_gate_classify(&gate,&(ev)); \
   halo_controller_glyph_input_event(&(ev)); \
   if((ev).type==SDL_EVENT_MOUSE_MOTION && (ev).motion.which!=SDL_TOUCH_MOUSEID) { \
    if(real_) host_menu_pointer_device_move(&pointer,(ev).motion.x,(ev).motion.y); \
    else host_menu_pointer_device_report(&pointer,(ev).motion.x,(ev).motion.y); } } while(0)
#define MOTION(px,py,rx,ry,id) do { memset(&motion,0,sizeof(motion)); motion.type=SDL_EVENT_MOUSE_MOTION; \
   motion.motion.which=(id);motion.motion.x=(px);motion.motion.y=(py);motion.motion.xrel=(rx);motion.motion.yrel=(ry); \
   e=motion; FEED(e); } while(0)
#define KEY() do { memset(&e,0,sizeof(e)); e.type=SDL_EVENT_KEY_DOWN; FEED(e); } while(0)
#define WINDOW(t) do { memset(&e,0,sizeof(e)); e.type=(t); FEED(e); } while(0)
#define TAKE() host_menu_pointer_read(&pointer,&out)
  MOTION(100,100,5,5,0);                                  /* a real mouse */
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_MOUSE);
  KEY();TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD);
  /* focus change, then the system's report: zero delta, then a warp, with Cocoa's global ID or not */
  WINDOW(SDL_EVENT_WINDOW_FOCUS_GAINED);WINDOW(SDL_EVENT_WINDOW_MOUSE_ENTER);
  MOTION(100,100,0,0,0);TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD && !out.moved);
  WINDOW(SDL_EVENT_WINDOW_FOCUS_GAINED);
  MOTION(500,400,400,300,0);TAKE();                       /* a warp or recenter */
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD && !out.moved && out.x==500);
  MOTION(500,400,0,0,0);TAKE();                           /* no change of position */
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD && !out.moved);
  /* touch-synthesized motion neither counts nor consumes the pending position report */
  WINDOW(SDL_EVENT_WINDOW_FOCUS_GAINED);
  MOTION(520,400,20,0,SDL_TOUCH_MOUSEID);TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD && !out.moved);
  MOTION(900,400,380,0,0);TAKE();                         /* still the pending report */
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD && !out.moved);
  /* pointer-enter before focus gained or after: either order leaves one report pending */
  WINDOW(SDL_EVENT_WINDOW_MOUSE_ENTER);WINDOW(SDL_EVENT_WINDOW_FOCUS_GAINED);
  MOTION(900,400,0,0,0);TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD && !out.moved);
  /* the user moves the mouse: both consumers switch on the same event */
  MOTION(910,400,10,0,0);TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_MOUSE && out.moved && out.radius==0);
  /* a relative-mode look (position fixed, deltas real) is movement */
  MOTION(910,400,6,0,0);TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_MOUSE && out.moved);
  /* SDL gives every pointer event its global ID (0) outside relative mode, real movement included: a move with
     that ID, with no click and no focus event before it, switches both consumers */
  KEY();TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_KEYBOARD);
  MOTION(930,400,20,0,0);TAKE();
  assert(glyph_menu_input.source==HALO_GLYPH_INPUT_MOUSE && out.moved);
  /* and a small step (a trackpad's slow drag) moves the menu pointer even where the legend's threshold waits */
  KEY();TAKE();
  MOTION(931,400,1,0,0);TAKE();
  assert(out.moved && out.x==931);
 }
 return 0;
}
