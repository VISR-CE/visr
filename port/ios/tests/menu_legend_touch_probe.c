#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
typedef int boolean;
typedef struct {short y0,x0,y1,x1;} rectangle2d;
typedef struct {short x,y;} point2d;
#define NONE (-1)
#define TRUE 1
#define FALSE 0
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) ((a)<0?-(a):(a))
#define UI_MOUSE_MAXIMUM_TARGETS 96
enum {_ui_mouse_target_item,_ui_mouse_target_value,_ui_mouse_target_list_slot,_ui_mouse_target_button};
enum {_ui_widget_type_text_box, _ui_widget_type_spinner_list, _widget_event_dpad_up=8, _widget_event_dpad_down};
struct ui_widget_definition {rectangle2d bounds;};
struct widget_instance {
 struct widget_instance *parent,*child,*next;
 short type,definition_tag_index,disabled,visible,horizontal_offset,vertical_offset,local_player_index,button;
 const char *name;
};
struct ui_mouse_target {struct widget_instance *widget;rectangle2d bounds;short kind,button_index;};
static struct ui_mouse_target ui_mouse_targets[96];
static long ui_mouse_target_count;
static int ui_mouse_noting_targets=1,hidden;
static short ui_mouse_radius;
static short __attribute__((unused)) local_player_index_for_draw_string_and_hack_in_icons=0;
static struct ui_widget_definition definitions[2]={{{0,0,26,21}},{{0,0,22,100}}};
static struct ui_widget_definition *ui_widget_definition_get(int i) {return &definitions[i];}
static short ui_mouse_key_button(struct widget_instance *w) {return w->button;}
static boolean ui_mouse_list_shows_several(struct widget_instance *w) {(void)w;return 0;}
static long ui_mouse_child_index(struct widget_instance *w) {(void)w;return NONE;}
static boolean widget_instance_can_receive_events(struct widget_instance *w) {(void)w;return 0;}
static boolean ui_mouse_widget_is_item(struct widget_instance *w) {(void)w;return 0;}
static void ui_mouse_note_carousel_arrows(struct widget_instance *w,point2d p) {(void)w;(void)p;}
static long ui_redraw_touch_target_units(void) {return 0;}
static int __attribute__((unused)) controller_glyph_hidden(short owner,short button) {
 assert(owner==0 && button>=0 && button<6);return hidden;
}
typedef float real;
typedef unsigned int pixel32;
static int draw_count;
static rectangle2d drawn_bounds;
static short ui_mouse_legend_label_width(struct widget_instance *l,struct ui_widget_definition const *d,short f) {(void)l;(void)d;(void)f;return 70;}
#define BITMAP_GROUP_TAG 1
struct bitmap_data {int width,height;};
struct rasterizer_dynamic_screen_geometry_parameters {int pad[8];};
static struct bitmap_data idle_bitmap={128,32},pressed_bitmap={128,32};
static int draw_frame=-1,missing_tag;
static const struct bitmap_data *drawn_bitmap;
static long tag_loaded(int group,const char *name) {
 assert(group==BITMAP_GROUP_TAG && !strcmp(name,"pc\\xbox\\bitmaps\\text_button_background"));
 return missing_tag?NONE:7;
}
static struct bitmap_data *bitmap_group_get_bitmap_from_sequence(long tag,short seq,short frame) {
 assert(tag==7 && seq==0 && (frame==0||frame==1));
 draw_frame=frame;return frame?&pressed_bitmap:&idle_bitmap;
}
static void csmemset(void *p,int v,size_t n) {memset(p,v,n);}
static int fast_ftol(float f) {return (int)f;}
static pixel32 last_color;
static rectangle2d drawn_clip;static int drawn_has_clip;
static void draw_bitmap_in_rect(struct bitmap_data *bitmap,rectangle2d const *rect,rectangle2d const *bitmap_rect,rectangle2d const *clip,pixel32 argb,struct rasterizer_dynamic_screen_geometry_parameters *params,boolean no_plasma) {
 (void)bitmap_rect;(void)params;(void)no_plasma;
 drawn_bitmap=bitmap;last_color=argb;draw_count++;drawn_bounds=*rect;
 drawn_has_clip=clip!=NULL;if(clip)drawn_clip=*clip;
}
/* GEOMETRY */
/* DRAW */
/* NOTE */
/* SEARCH */
int main(void) {
 struct widget_instance parent={0};
 struct widget_instance button={.parent=&parent,.visible=1,.horizontal_offset=346,.vertical_offset=414,.local_player_index=NONE,.button=1};
 struct widget_instance caption={.parent=&parent,.type=_ui_widget_type_text_box,.visible=1,.definition_tag_index=1,.horizontal_offset=368,.vertical_offset=414,.button=NONE};
 parent.child=&button;button.next=&caption;
 point2d offset={346,414};
 /* Controller and keyboard legends retain their original icon-plus-label bounds. */
 ui_mouse_note_target(&button,&definitions[0],offset);
 assert(ui_mouse_target_count==1);
 assert(ui_mouse_targets[0].bounds.x0==346 && ui_mouse_targets[0].bounds.x1==468);
 assert(ui_mouse_targets[0].bounds.y0==414 && ui_mouse_targets[0].bounds.y1==440);
 hidden=1;ui_mouse_target_count=0;
 ui_mouse_note_target(&button,&definitions[0],offset);
 struct ui_mouse_target *target=&ui_mouse_targets[0];
 assert(target->button_index==1 && target->kind==_ui_mouse_target_button);
 /* 60 menu units cover 46.875 pt at 375/480 pt per menu unit. */
 assert(target->bounds.y1-target->bounds.y0>=60);
 assert(target->bounds.x1-target->bounds.x0>=60);
 assert(ui_mouse_target_at(420,398)==target);
 assert(ui_mouse_target_at(420,455)==target);
 assert(!ui_mouse_target_at(420,390));
 assert(!ui_mouse_target_at(480,427));
 /* The visible button sits inside the 35-unit legend band (y 410-445), is centered on the
  text (70 wide) and is inside its pointer target. */
 rectangle2d visual,hit=definitions[0].bounds;
 assert(ui_mouse_legend_bounds(&button,&definitions[0],offset,&hit,&visual));
 assert(!memcmp(&hit,&target->bounds,sizeof(hit)));
 assert(visual.y1-visual.y0==28 && visual.y0>=410 && visual.y1<=445);
 assert(visual.x0>=hit.x0 && visual.x1<=hit.x1 && visual.y0>=hit.y0 && visual.y1<=hit.y1);
 assert((visual.x0+visual.x1)/2==368+70/2);
 /* The button is the game's own text_button_background art: frame 0 idle, frame 1 pressed. */
 ui_widget_draw_touch_legend(&visual,NULL,1,0);
 assert(draw_count==1 && draw_frame==0 && drawn_bitmap==&idle_bitmap);
 assert(!memcmp(&drawn_bounds,&visual,sizeof(visual)) && !drawn_has_clip);
 ui_widget_draw_touch_legend(&visual,NULL,1,1);
 assert(draw_frame==1 && drawn_bitmap==&pressed_bitmap);
 rectangle2d clip={410,360,440,450};
 ui_widget_draw_touch_legend(&visual,&clip,1,0);
 assert(drawn_has_clip && !memcmp(&drawn_clip,&clip,sizeof(clip)));
 draw_count=0;missing_tag=1;
 ui_widget_draw_touch_legend(&visual,NULL,1,0);
 assert(draw_count==0);missing_tag=0;
 for(int face=0;face<6;face++) {
  button.button=face;ui_mouse_target_count=0;
  ui_mouse_note_target(&button,&definitions[0],offset);
  assert(target->button_index==face);
  assert(target->bounds.y1-target->bounds.y0>=60);
 }
 button.button=1;
 /* Even a short caption receives enough width. */
 definitions[1].bounds.x1=10;ui_mouse_target_count=0;
 ui_mouse_note_target(&button,&definitions[0],offset);
 assert(target->bounds.x1-target->bounds.x0>=60);
 assert(target->button_index==1);
 button.disabled=1;ui_mouse_target_count=0;
 ui_mouse_note_target(&button,&definitions[0],offset);
 assert(ui_mouse_target_count==0);
 return 0;
}
