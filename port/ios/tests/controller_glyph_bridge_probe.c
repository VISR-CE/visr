#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#undef TARGET_OS_TV
#define TARGET_OS_TV GLYPH_TEST_TV
#include "halo_controller_glyphs.h"
#define SDL_GetGamepadType glyph_type
#define SDL_GetGamepadAppleSFSymbolsNameForButton glyph_button
#define SDL_GetGamepadAppleSFSymbolsNameForAxis glyph_axis
static int _handle_gamepad, keyboard, owns, type=SDL_GAMEPAD_TYPE_PS5, missing;
static SDL_Gamepad *physical=(void *)(uintptr_t)31;
static void *handle_get(uint32_t handle,int kind) {(void)kind;return handle ? (void *)(uintptr_t)handle : NULL;}
static SDL_Gamepad *host_ios_gamepad_physical(SDL_Gamepad *pad) {assert(pad==(void *)(uintptr_t)21);return physical;}
static int host_menu_keyboard(void) {return keyboard;}
static int host_spatial_owns_sense_controllers(void) {return owns;}
static int host_spatial_controller_glyph(int button,int menus,char *label,unsigned int size) {
 (void)button;assert(menus==1);snprintf(label,size,"sense.circle");return !missing;
}
static SDL_GamepadType glyph_type(SDL_Gamepad *pad) {assert(pad==physical);return type;}
static const char *glyph_button(SDL_Gamepad *pad,SDL_GamepadButton button) {
 assert(pad==physical && button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);return missing?NULL:"l1.rectangle.roundedbottom";
}
static const char *glyph_axis(SDL_Gamepad *pad,SDL_GamepadAxis axis) {
 assert(pad==physical && axis==SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);return missing?NULL:"r2.rectangle.roundedtop";
}
/* BRIDGE */
int main(void) {
 char label[96];
 assert(host_sdl_gamepad_glyph(21,5,"",label,sizeof(label),0,0)==2 && !strcmp(label,"l1.rectangle.roundedbottom"));
 assert(host_sdl_gamepad_glyph(21,7,"",label,sizeof(label),0,0)==2 && !strcmp(label,"r2.rectangle.roundedtop"));
 type=SDL_GAMEPAD_TYPE_XBOXONE;assert(host_sdl_gamepad_glyph(21,5,"",label,sizeof(label),0,0)==0);
 type=SDL_GAMEPAD_TYPE_PS5;missing=1;assert(host_sdl_gamepad_glyph(21,5,"",label,sizeof(label),0,0)==0);missing=0;
 assert(host_sdl_gamepad_glyph(21,0,"playstation",label,sizeof(label),0,0)==2 && !strcmp(label,"xmark.circle"));
 assert(host_sdl_gamepad_glyph(21,0,"switch",label,sizeof(label),0,0)==2 && !strcmp(label,"b.circle"));
 assert(host_sdl_gamepad_glyph(21,0,"mfi",label,sizeof(label),0,0)==2 && !strcmp(label,"a.circle"));
 assert(host_sdl_gamepad_glyph(21,0,"touch",label,sizeof(label),0,0)==1);
 assert(host_sdl_gamepad_glyph(21,0,"keyboard",label,sizeof(label),0,0)==3);
 physical=NULL;keyboard=1;
#if TARGET_OS_TV
 assert(host_sdl_gamepad_glyph(21,0,"",label,sizeof(label),0,0)==0); /* remote fallback */
#else
 assert(host_sdl_gamepad_glyph(21,0,"",label,sizeof(label),0,0)==1); /* active touch */
#endif
 assert(host_sdl_gamepad_glyph(0,0,"",label,sizeof(label),0,0)==3);
 owns=1;assert(host_sdl_gamepad_glyph(0,1,"",label,sizeof(label),0,1)==2 && !strcmp(label,"sense.circle"));
 missing=1;assert(host_sdl_gamepad_glyph(0,1,"",label,sizeof(label),0,1)==0);
 assert(host_sdl_gamepad_glyph(0,1,"",label,sizeof(label),1,1)==0); /* second player is not Sense */
 return 0;
}
