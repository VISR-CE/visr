#include <assert.h>
#include <stddef.h>
#include <string.h>
#define NONE (-1)
/* GLYPH BLENDS */
#define FALSE 0
typedef struct {short y0,x0,y1,x1;} rectangle2d;
struct bitmap_data {int unused;};
enum {_gamepad_analog_button_a,_gamepad_analog_button_b};
static short virtual_keyboard_glyph_player=2;
static int ready[2],hidden[2],originals,patches,glyphs,strips;
static int patch_right[2];
static int controller_glyph_hidden(short player,short physical) { assert(player==2);return hidden[physical]; }
static int controller_glyph_ready(short player,short semantic,short physical,rectangle2d const *bounds) {
 assert(player==2 && semantic==-1 && bounds->y0==414);return ready[physical];
}
static void controller_glyph_blit(struct bitmap_data *b,rectangle2d const *bounds,rectangle2d const *source,
 rectangle2d const *clip,unsigned long color,short blend) {
 (void)b;(void)bounds;assert(color==0xffffffff && blend==0);
 if(clip)strips++;else {assert(source->x0==0 && source->y0==414);patch_right[patches++]=bounds->x1;}
}
static int controller_glyph_draw(short player,short semantic,short physical,rectangle2d const *bounds,
 rectangle2d const *clip,unsigned long color,short blend) {
 (void)physical;assert(player==2 && semantic==-1 && bounds->y0==414 && !clip);
 assert(color==0xff2996ff && blend==0);glyphs++;return 1;
}
static void draw_bitmap_in_rect(struct bitmap_data *b,rectangle2d *bounds,rectangle2d *source,
 rectangle2d *clip,unsigned long color,void *params,int plasma) {
 (void)b;(void)bounds;(void)source;(void)clip;(void)color;(void)params;(void)plasma;originals++;
}
/* BACKGROUND */
int main(void) {
 struct bitmap_data b;rectangle2d bounds={0,0,480,640};
 virtual_keyboard_draw_background(&b,&bounds,"ui\\bkd_virtual_keyboard");
 assert(originals==1 && !patches && !glyphs && !strips); /* Xbox and missing: unchanged */
 ready[0]=ready[1]=1;virtual_keyboard_draw_background(&b,&bounds,"ui\\bkd_virtual_keyboard");
 assert(originals==1 && patches==2 && glyphs==2 && strips==5);
 ready[0]=0;patches=glyphs=strips=0;virtual_keyboard_draw_background(&b,&bounds,"ui\\bkd_virtual_keyboard");
 assert(originals==1 && patches==1 && glyphs==1 && strips==5);
 ready[0]=ready[1]=hidden[0]=hidden[1]=1;patches=glyphs=strips=0;
 virtual_keyboard_draw_background(&b,&bounds,"ui\\bkd_virtual_keyboard");
 assert(patch_right[0]==408 && patch_right[1]==509);
 patches=glyphs=strips=0;virtual_keyboard_draw_background(&b,&bounds,"custom\\keyboard");
 assert(originals==2 && !patches && !glyphs && !strips);
 return 0;
}
