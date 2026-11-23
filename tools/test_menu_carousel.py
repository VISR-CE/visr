"""Run the production carousel's state, pointer, and render orchestration in C.

Game-data callbacks and drawing are narrow stubs; carousel functions come from
ui_widget.c so binding restoration and target clipping regressions execute here.
"""

from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import SkipTest

ROOT = Path(__file__).resolve().parents[1]


def _function(source, signature):
    start = source.index(signature + "(")
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


PREAMBLE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "halo_ui_carousel.h"
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define csmemset memset
#define _ui_widget_type_spinner_list 1
#define _ui_audio_feedback_flag_failure 1
typedef struct { short y0,x0,y1,x1; } rectangle2d;
typedef struct { short x,y; } point2d;
struct ui_widget_definition { struct {int count;} child_widgets; rectangle2d bounds; };
struct widget_instance { long creation_time; int type,definition_tag_index; short horizontal_offset,vertical_offset; struct {struct {short selected_index;int number_of_items;} list;} parameters; struct widget_instance *child,*next,*focused_child,*parent; struct {int number_of_sprite_frames,current_frame_index;} animation; int visible,bound,disabled,can_receive; };
static struct ui_widget_definition defs[4];
static unsigned long now;
static struct { unsigned long current_system_milliseconds; } widget_globals;
static unsigned long system_milliseconds(void) { return now; }
static struct ui_widget_definition *ui_widget_definition_get(int i) { return &defs[i]; }
static struct widget_instance *widget_instance_get_nth_child(struct widget_instance *w,int n) { w=w->child; while(w && n-->0)w=w->next; return w; }
static void widget_instance_give_focus_directly(struct widget_instance *w,struct widget_instance *c){w->focused_child=c;}
static boolean widget_instance_can_receive_events(struct widget_instance *w) {return w->can_receive;}
static void ui_play_audio_feedback_sound(int s){(void)s;}
enum { _ui_mouse_target_list_slot, _ui_mouse_target_carousel_peek };
struct ui_mouse_target { struct widget_instance *widget; rectangle2d bounds; short kind,button_index; };
#define UI_MOUSE_MAXIMUM_TARGETS 64
static struct ui_mouse_target ui_mouse_targets[64];
static int ui_mouse_target_count,ui_mouse_noting_targets=1,ui_mouse_click_pending;
struct halo_ui_pointer { int x,y,radius,down,held,wheel_steps,left_clicks; };
'''


STUBS = r'''
static int halo_menu_pointer_controls(void) { return 0; }
static int a_presses;
#define _gamepad_analog_button_a 0
static void ui_mouse_press(int button) {assert(button==_gamepad_analog_button_a);a_presses++;}
static void ui_mouse_give_focus(struct widget_instance *w) {(void)w;}
static int drawn[64],drawn_count;
static void ui_carousel_rebind(struct widget_instance *w) { int slot=0; for(struct widget_instance*c=w->child;c;c=c->next) c->bound=ui_widget_port_carousel_first(w)+slot++; }
static void spinner_list_update(struct widget_instance *w) { for(struct widget_instance*c=w->child;c;c=c->next)c->animation.current_frame_index=c==w->focused_child; }
static void widget_instance_render_recursive(struct widget_instance *w,rectangle2d *r,point2d o,boolean f,boolean fx) {(void)r;(void)o;(void)f;(void)fx;drawn[drawn_count++]=w->bound;}
'''


CHECKS = r'''
int main(void) {
 struct widget_instance w={0},c[3]={{0}};
 defs[0].child_widgets.count=3;
 for(int i=0;i<3;i++){c[i].definition_tag_index=i+1;c[i].parent=&w;c[i].visible=1;c[i].animation.number_of_sprite_frames=2;defs[i+1].bounds=(rectangle2d){100,38,300,196}; if(i<2)c[i].next=&c[i+1];}
 w.can_receive=1;w.type=1;w.child=c;w.focused_child=&c[2];w.parameters.list.selected_index=2;w.parameters.list.number_of_items=10;
 struct ui_carousel_state *state=ui_carousel_state(&w);
 state->first=state->binding_first=0;halo_ui_drag_reset(&state->drag,0);
 assert(ui_carousel_controller_move(&w,1));assert(w.parameters.list.selected_index==3 && state->first==1 && w.focused_child==&c[2]);
 halo_ui_drag_reset(&state->drag,2*167);state->first=state->binding_first=2;w.parameters.list.selected_index=3;w.focused_child=&c[1];
 ui_carousel_render(&w,(point2d){0,0});
 assert(drawn_count==5);for(int i=0;i<5;i++)assert(drawn[i]==i+1);
 assert(state->binding_first==2 && w.parameters.list.selected_index==3 && w.focused_child==&c[1] && ui_carousel_clip==NULL && ui_mouse_noting_targets);
 assert(ui_mouse_target_count==3);for(int i=0;i<3;i++)assert(ui_mouse_targets[i].button_index==i+2);
 struct halo_ui_pointer p={.x=250,.y=150,.down=1,.held=1};
 assert(ui_carousel_pointer(&p));assert(w.parameters.list.selected_index==3 && state->drag.state==HALO_UI_DRAG_PRESSED);
 p.x=230;now+=16;assert(ui_carousel_pointer(&p));p.x=0;now+=16;assert(ui_carousel_pointer(&p));
 p.down=0;p.held=0;p.left_clicks=1;p.wheel_steps=1;now+=16;assert(ui_carousel_pointer(&p));assert(!p.left_clicks && !p.wheel_steps && !ui_carousel_capture);
 halo_ui_drag_reset(&state->drag,7*167);w.parameters.list.number_of_items=3;now+=16;widget_globals.current_system_milliseconds=now;ui_carousel_tick(&w);assert(state->drag.offset==0 && state->first==0);

 /* A tap-release leaves exactly the host click for ordinary target dispatch. */
 p=(struct halo_ui_pointer){.x=250,.y=150,.down=1,.held=1};now+=16;assert(ui_carousel_pointer(&p));
 p.x=256;p.y=154;p.down=0;p.held=0;p.left_clicks=1;now+=16;assert(ui_carousel_pointer(&p));assert(p.left_clicks==1);
 /* Multiple render calls within one menu frame cannot accelerate a glide. */
 w.parameters.list.number_of_items=10;halo_ui_drag_reset(&state->drag,0);halo_ui_drag_glide(&state->drag,167);widget_globals.current_system_milliseconds+=16;
 ui_carousel_tick(&w);float once=state->drag.offset;ui_carousel_tick(&w);assert(state->drag.offset==once);

 /* A narrower frame clips both peeks and full-card targets to the drawn band. */
 halo_ui_drag_reset(&state->drag,2*167);state->first=state->binding_first=2;
 state->frame=(rectangle2d){120,100,250,600};ui_mouse_target_count=0;
 ui_mouse_note_carousel_peeks(&w,(point2d){0,0});
 assert(ui_mouse_target_count==2);
 assert(ui_mouse_targets[0].bounds.x0==100 && ui_mouse_targets[0].bounds.x1==232);
 assert(ui_mouse_targets[0].button_index==-1);
 assert(ui_mouse_targets[1].bounds.x0==575 && ui_mouse_targets[1].bounds.x1==600);
 assert(ui_mouse_targets[1].button_index==1);
 ui_carousel_render(&w,(point2d){0,0});assert(ui_mouse_target_count==4);
 for(int i=0;i<4;i++) {
  rectangle2d r=ui_mouse_targets[i].bounds;
  assert(r.x0>=100 && r.x1<=600 && r.y0==120 && r.y1==250);
 }
 assert(ui_mouse_targets[2].button_index==3 && ui_mouse_targets[3].button_index==4);
 /* Disabled lists and lists without events do not create full-card targets. */
 ui_mouse_target_count=0;w.disabled=1;ui_carousel_render(&w,(point2d){0,0});assert(ui_mouse_target_count==0);
 w.disabled=0;w.can_receive=0;ui_carousel_render(&w,(point2d){0,0});assert(ui_mouse_target_count==0);
 w.can_receive=1;c[0].disabled=1;ui_carousel_render(&w,(point2d){0,0});
 assert(ui_mouse_target_count>0);
 for(int i=0;i<ui_mouse_target_count;i++)assert(!ui_mouse_targets[i].widget->disabled);
 c[0].disabled=0;
 /* Direct activation selects an absolute item and posts exactly one A. */
 struct ui_mouse_target tap={.widget=&c[0],.button_index=4};
 ui_carousel_activate(&tap);assert(w.parameters.list.selected_index==4 && w.focused_child==&c[2] && a_presses==1);
 ui_carousel_peek(&w,1);assert(state->drag.target==3*167 && w.parameters.list.selected_index==4 && a_presses==1);
 ui_carousel_forget(&w);assert(!ui_carousel_capture);
 puts("extracted carousel integration probe: ok");
}
'''


def test_carousel_production_integration():
    compiler = shutil.which("clang") or shutil.which("cc")
    if not compiler:
        raise SkipTest("A C compiler is required for the carousel integration probe")
    source = (ROOT / "source/interface/ui_widget.c").read_text()
    start = source.index("/* port: three reusable widgets")
    assert source.index("#define widget_globals ") < start
    end = source.index("/* port: carousel state helpers end. */", start)
    harness = "\n".join((
        PREAMBLE,
        source[start:end],
        STUBS,
        "enum halo_menu_focus_item { HALO_MENU_FOCUS_ROW, HALO_MENU_FOCUS_CARD };",
        _function(source, "int ui_widget_port_draws_focus"),
        _function(source, "static enum halo_menu_focus_item ui_list_focus_item"),
        _function(source, "static boolean ui_list_child_draws_focus"),
        _function(source, "static boolean ui_carousel_pointer"),
        _function(source, "static void ui_mouse_note_carousel_peeks"),
        _function(source, "static void ui_carousel_render"),
        _function(source, "static void ui_carousel_activate"),
        _function(source, "static void ui_carousel_peek"),
        CHECKS,
    ))
    with tempfile.TemporaryDirectory() as directory:
        probe = Path(directory) / "probe.c"
        binary = Path(directory) / "probe"
        probe.write_text(harness)
        subprocess.run([
            compiler, "-O2", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-I" + str(ROOT / "port/runtime/include"),
            str(probe), "-lm", "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    test_carousel_production_integration()
