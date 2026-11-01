#include <assert.h>
#include <stdlib.h>
#include <wchar.h>
#define NONE (-1)
typedef struct {short y0,x0,y1,x1;} rectangle2d;
struct ui_widget_definition {rectangle2d bounds;};
struct widget_instance {int definition_tag_index,visible;short horizontal_offset,vertical_offset,local_player_index,button;struct widget_instance *parent,*child,*next;};
static short local_player_index_for_draw_string_and_hack_in_icons;
static struct ui_widget_definition definitions[2];
static int hidden,requests;
static struct ui_widget_definition *ui_widget_definition_get(int index) {return &definitions[index];}
static short ui_mouse_key_button(struct widget_instance *w) {return w->button;}
static int controller_glyph_hidden(short player,short button) {assert(player==2 && button==0);requests++;return hidden;}
/* CAPTION */
int main(void) {
 struct widget_instance parent={0},button={.definition_tag_index=0,.visible=1,.horizontal_offset=20,.vertical_offset=414,.local_player_index=2,.button=0},caption={.definition_tag_index=1,.visible=1,.horizontal_offset=42,.vertical_offset=414,.local_player_index=2,.button=-1};
 parent.child=&button;button.parent=caption.parent=&parent;button.next=&caption;
 assert(!wcscmp(ui_widget_touch_caption(&caption,L"=SELECT"),L"=SELECT"));
 hidden=1;assert(!wcscmp(ui_widget_touch_caption(&caption,L"= SELECT"),L"SELECT"));
 int previous=requests;assert(!wcscmp(ui_widget_touch_caption(&caption,L"SELECT"),L"SELECT") && requests==previous);
 button.horizontal_offset=200;assert(!wcscmp(ui_widget_touch_caption(&caption,L"=SELECT"),L"=SELECT"));
 return 0;
}
