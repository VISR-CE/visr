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
static pixel32 modulate_pixel32_by_real_alpha(pixel32 color, real alpha) {(void)alpha;return color;}
static void draw_quad(rectangle2d *bounds,pixel32 color) {
 assert(color==0xff2996ff || color==0xb0102030);
 if(!draw_count++)drawn_bounds=*bounds;
 drawn_bounds.x0=MIN(drawn_bounds.x0,bounds->x0);
 drawn_bounds.x1=MAX(drawn_bounds.x1,bounds->x1);
 drawn_bounds.y0=MIN(drawn_bounds.y0,bounds->y0);
 drawn_bounds.y1=MAX(drawn_bounds.y1,bounds->y1);
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
 /* Drawing and hit testing use the identical screen-space rectangle. */
 rectangle2d visual=definitions[0].bounds;
 assert(ui_mouse_legend_bounds(&button,&definitions[0],offset,&visual));
 assert(!memcmp(&visual,&target->bounds,sizeof(visual)));
 ui_widget_draw_touch_legend(&visual,NULL,1);
 assert(draw_count>0 && !memcmp(&visual,&drawn_bounds,sizeof(visual)));
 rectangle2d clip={410,360,440,450};draw_count=0;
 ui_widget_draw_touch_legend(&visual,&clip,1);
 assert(drawn_bounds.x0>=clip.x0 && drawn_bounds.x1<=clip.x1);
 assert(drawn_bounds.y0>=clip.y0 && drawn_bounds.y1<=clip.y1);
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
