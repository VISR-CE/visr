#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "menu_geometry.h"
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
enum {_ui_mouse_target_item,_ui_mouse_target_value,_ui_mouse_target_list_slot,_ui_mouse_target_button,_ui_mouse_target_action};
enum {_ui_widget_type_text_box, _ui_widget_type_spinner_list, _widget_event_dpad_up=8, _widget_event_dpad_down};
enum {_gamepad_analog_button_a,_gamepad_analog_button_b,_gamepad_analog_button_x,_gamepad_analog_button_y};
struct string_list_ref {long index;};
struct ui_widget_definition {rectangle2d bounds,list_header_bounds,list_footer_bounds;struct string_list_ref text_label_string_list;struct {long index;} text_font;short horizontal_offset,justification;};
enum {_text_justification_left,_text_justification_right,_text_justification_center};
struct widget_instance {
 struct widget_instance *parent,*child,*next;
 short type,definition_tag_index,disabled,visible,horizontal_offset,vertical_offset,local_player_index,button;
 const char *name;
};
struct ui_mouse_target {struct widget_instance *widget;rectangle2d bounds;short kind,button_index,back_arrow_end,forward_arrow_start;rectangle2d value_bounds;};
static struct ui_mouse_target ui_mouse_targets[96];
static long ui_mouse_target_count;
static int ui_mouse_noting_targets=1,hidden;
static short ui_mouse_radius;
static short __attribute__((unused)) local_player_index_for_draw_string_and_hack_in_icons=0;
static struct ui_widget_definition definitions[2]={{{0,0,26,21},{NONE}},{{0,0,22,100},{5}}};
static struct ui_widget_definition *ui_widget_definition_get(int i) {return &definitions[i];}
static short ui_mouse_key_button(struct widget_instance *w) {return w->button;}
static boolean ui_mouse_list_shows_several(struct widget_instance *w) {(void)w;return 0;}
static long ui_mouse_child_index(struct widget_instance *w) {(void)w;return NONE;}
static boolean widget_instance_can_receive_events(struct widget_instance *w) {(void)w;return 0;}
static boolean ui_mouse_widget_is_item(struct widget_instance *w) {(void)w;return 0;}
static void ui_mouse_note_carousel_peeks(struct widget_instance *w,point2d p) {(void)w;(void)p;}
static long ui_redraw_touch_target_units(void) {return 0;}
#define SPINNER_EXTRA_WIDTH 0
#define HALO_MENU_SLIDER_WIDTH 0
enum {HALO_ROW_NONE,HALO_ROW_TOGGLE,HALO_ROW_RANGE};
struct halo_menu_row {int kind;};
static struct halo_menu_row ui_widget_row_control(struct widget_instance *w) {(void)w;struct halo_menu_row r={HALO_ROW_NONE};return r;}
static int ui_widget_row_has_dropdown(struct widget_instance *w) {(void)w;return 0;}
static int halo_menu_pointer_controls(void) {return 0;}
static int spinner_string_list_extra_count(long index) {(void)index;return 0;}
static int __attribute__((unused)) controller_glyph_hidden(short owner,short button) {
 assert(owner==0 && button>=0 && button<6);return hidden;
}
typedef float real;
typedef unsigned int pixel32;
static int draw_count;
static short ui_mouse_legend_label_width(struct widget_instance *l,struct ui_widget_definition const *d,short f) {(void)l;(void)d;(void)f;return 70;}
static long draw_string_get_font(void) {return NONE;}
static void draw_string_set_font(long f) {(void)f;}
static void draw_string_set_format(int a,int b,int c) {(void)a;(void)b;(void)c;}
static void draw_string_set_indents(int a,int b) {(void)a;(void)b;}
typedef int wchar_stub;
/* every caption measures 70 wide */
static short ui_widget_port_text_width(wchar_t const *t) {(void)t;return 70;}
static float stub_bottom=480.0f,stub_ppu=0.78125f;
void halo_screen_menu_visible_bottom(float *b,float *p) {*b=stub_bottom;*p=stub_ppu;}
#define BITMAP_GROUP_TAG 1
struct bitmap_data {int width,height;};
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
#define NUMBER_OF_POINTS_PER_RECTANGLE 4
typedef struct {float i,j;} real_point2d_stub;
struct dynamic_screen_vertex {pixel32 color;struct {float x,y;} texture_coordinates;struct {float x,y;} position;};
struct rasterizer_dynamic_screen_geometry_parameters {struct {float i,j;} map_texture_scale[2],map_scale[2];struct bitmap_data *map[2];void *meter_parameters;int point_sampled,alpha_weighted,framebuffer_blend_function;};
static pixel32 last_color;
static int slice_count;static float slice_x0[4],slice_x1[4],slice_u0[4],slice_u1[4],slice_y0,slice_y1;
static void rasterizer_psuedo_dynamic_screen_quad_draw(struct rasterizer_dynamic_screen_geometry_parameters *p,struct dynamic_screen_vertex *v) {
 drawn_bitmap=p->map[0];last_color=v[0].color;draw_count++;
 slice_x0[slice_count]=v[0].position.x;slice_x1[slice_count]=v[1].position.x;
 slice_u0[slice_count]=v[0].texture_coordinates.x;slice_u1[slice_count]=v[1].texture_coordinates.x;
 slice_y0=v[0].position.y;slice_y1=v[2].position.y;slice_count++;
}
/* SLOTS */
/* GEOMETRY */
/* SLICE */
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
 /* B's slot (the bar's, menu_geometry.h) sits second from the right.
  Its pointer target runs from the button's top (412) to the screen's bottom edge (480): 68 units,
  53 pt at 375/480 pt per unit, and never reaches above the bar's top rule (404). */
 assert(target->bounds.y1-target->bounds.y0>=60);
 assert(target->bounds.y0==412 && target->bounds.y1==480);
 assert((target->bounds.y1-target->bounds.y0)*0.78125f>=44);
 assert(target->bounds.x1-target->bounds.x0>=60);
 assert(ui_mouse_target_at(420,415)==target);
 assert(ui_mouse_target_at(420,479)==target);
 assert(!ui_mouse_target_at(420,398));
 assert(!ui_mouse_target_at(480,427));
 /* The visible button is 40 units tall and is the slot itself (MENU_BAR_SLOT_B_X0 .. _X1),
  with its caption centered by the legend's caption shift, and is inside its pointer target. */
 rectangle2d visual,hit=definitions[0].bounds;
 assert(ui_mouse_legend_bounds(&button,&definitions[0],offset,&hit,&visual,NULL));
 assert(!memcmp(&hit,&target->bounds,sizeof(hit)));
 assert(visual.y1-visual.y0==40 && visual.y0>=404 && visual.y1<=456);
 assert(visual.x0>=hit.x0 && visual.x1<=hit.x1 && visual.y0>=hit.y0 && visual.y1<=hit.y1);
 assert(visual.x0==MENU_BAR_SLOT_B_X0 && visual.x1==MENU_BAR_SLOT_B_X1);
 assert(ui_touch_legend_caption_shift(&caption)==(visual.x0+visual.x1)/2-70/2-368);
 /* Every slot has the same place on every screen: the A, B, X and Y buttons' pointer targets never
  overlap each other, stay below the bar's top rule, reach the screen's bottom and are 44 pt tall,
  also on a short screen whose backdrop margin is cropped (visible bottom 462, 0.9 pt per unit). */
 for(int crop=0;crop<2;crop++) {
  stub_bottom=crop?462.0f:480.0f;stub_ppu=crop?0.9f:0.78125f;
  rectangle2d rects[4],visuals[4];
  for(int face=0;face<4;face++) {
   button.button=face;ui_mouse_target_count=0;
   ui_mouse_note_target(&button,&definitions[0],offset);
   rects[face]=target->bounds;
   hit=definitions[0].bounds;
   assert(ui_mouse_legend_bounds(&button,&definitions[0],offset,&hit,&visuals[face],NULL));
   assert(rects[face].y0>=404 && rects[face].y1==(short)ceilf(stub_bottom));
   assert((rects[face].y1-rects[face].y0)*stub_ppu>=44-stub_ppu);
   assert(visuals[face].x0>=rects[face].x0 && visuals[face].x1<=rects[face].x1);
  }
  for(int i=0;i<4;i++) for(int j=i+1;j<4;j++) {
   assert(rects[i].x1<=rects[j].x0 || rects[j].x1<=rects[i].x0);
   assert(visuals[i].x1<=visuals[j].x0 || visuals[j].x1<=visuals[i].x0);
  }
 }
 stub_bottom=480.0f;stub_ppu=0.78125f;button.button=1;ui_mouse_target_count=0;
 ui_mouse_note_target(&button,&definitions[0],offset);
 hit=definitions[0].bounds;ui_mouse_legend_bounds(&button,&definitions[0],offset,&hit,&visual,NULL);
 /* The button is the game's own text_button_background art: frame 0 idle, frame 1 pressed. */
 slice_count=0;ui_widget_draw_touch_legend(&visual,NULL,1,0);
 assert(draw_count==3 && slice_count==3 && draw_frame==0 && drawn_bitmap==&idle_bitmap);
 /* three slices tile the button; the caps keep the picture's height at the button's height */
 assert(slice_x0[0]==visual.x0 && slice_x1[2]==visual.x1 && slice_x1[0]==slice_x0[1] && slice_x1[1]==slice_x0[2]);
 assert(slice_y0==visual.y0 && slice_y1==visual.y1);
 assert(slice_x1[0]-slice_x0[0]==20 && slice_x1[2]-slice_x0[2]==20);
 assert(slice_u0[0]==0 && slice_u1[0]==0.125f && slice_u0[2]==0.875f && slice_u1[2]==1);
 slice_count=0;ui_widget_draw_touch_legend(&visual,NULL,1,1);
 assert(draw_frame==1 && drawn_bitmap==&pressed_bitmap);
 draw_count=0;missing_tag=1;
 slice_count=0;ui_widget_draw_touch_legend(&visual,NULL,1,0);
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
