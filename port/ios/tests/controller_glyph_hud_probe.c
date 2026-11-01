#include <assert.h>
#include <stddef.h>
#define fast_ftol(v) ((long)(v))
#define TRUE 1
/* GLYPH BLENDS */
/* RENDERER BLENDS */
#define FALSE 0
#define TEST_FLAG(v,b) (((v)>>(b))&1)
typedef int boolean;
typedef float real;
typedef unsigned long pixel32;
typedef struct {short y0,x0,y1,x1;} rectangle2d;
typedef struct {short x,y;} point2d;
typedef struct {float y0,x0,y1,x1;} real_rectangle2d;
struct bitmap_data {int width,height;};
struct icon_hud_element_definition {short sequence_index,width_offset;point2d offset;pixel32 color;int frame_rate,flags;};
static struct {struct {struct {long index;} messaging_icons;} messaging;} globals;
#define hud_globals (&globals)
enum {_hud_icon_use_text_bit,_hud_icon_use_color_bit,_hud_icon_absolute_width_bit,_hud_corner_bottom_left};
static int fallbacks,handled=1,seen_physical,seen_semantic,seen_player;
static pixel32 seen_color;
static rectangle2d seen_bounds;
static struct bitmap_data bitmap={64,32};
static real_rectangle2d sprite={0,.25f,.5f,.5f};
static int game_time_get(void) {return 1;}
static int local_player_count(void) {return 2;}
static void hud_retrieve_bitmap_and_bounding_rect(long group,short sequence,long frame,
 struct bitmap_data const **out,real_rectangle2d const **clip) {
 (void)group;(void)sequence;(void)frame;*out=&bitmap;*clip=&sprite;
}
static int _texture_cache_bitmap_get_hardware_format(struct bitmap_data *b,int block,int load) {(void)b;(void)block;(void)load;return 1;}
static int controller_glyph_draw(short player,short semantic,short physical,rectangle2d const *b,
 rectangle2d const *clip,pixel32 color,short blend) {
 assert(clip==NULL && blend==_shader_framebuffer_blend_function_alpha_multiply_add);
 assert(blend>=0 && blend<NUMBER_OF_SHADER_FRAMEBUFFER_BLEND_FUNCTIONS);seen_player=player;seen_semantic=semantic;seen_physical=physical;
 seen_bounds=*b;seen_color=color;return handled;
}
static void hud_draw_bitmap_direct(struct bitmap_data const *b,short placement,point2d const *point,
 real_rectangle2d const *clip,real scale,real theta,pixel32 color,boolean interface) {
 (void)b;(void)placement;(void)point;(void)clip;(void)scale;(void)theta;(void)interface;
 assert(color==0xff00ff00);fallbacks++;
}
/* HUD */
int main(void) {
 rectangle2d bounds={0,0,100,100},cursor={0,20,50,100};
 struct icon_hud_element_definition icon={0,2,{4,8},0xff00ff00,0,1<<_hud_icon_use_color_bit};
 render_state_bitmap(&bounds,&cursor,0xff2996ff,&icon,2,18,1);
 assert(!fallbacks && seen_physical==2 && seen_semantic==18 && seen_player==1);
 assert(seen_color==0xff2996ff && seen_bounds.x0==23 && seen_bounds.x1==35);
 assert(seen_bounds.y1==44 && seen_bounds.y0==32);
 assert(cursor.x0==36); /* original sprite advance retained at split-screen scale */
 handled=0;render_state_bitmap(&bounds,&cursor,0xff2996ff,&icon,2,18,1);assert(fallbacks==1);
 handled=1;render_state_bitmap(&bounds,&cursor,0xff2996ff,&icon,-1,-1,1);assert(fallbacks==2);
 return 0;
}
