"""Delete completion clears editing and skips the deleted item's editor."""
from pathlib import Path
import subprocess

from test_menu_cancel import block

ROOT = Path(__file__).resolve().parents[1]


def test_delete_completion(tmp_path):
    events = (ROOT / 'source/interface/ui_widget_event_handler_functions.c').read_text()
    widgets = (ROOT / 'source/interface/ui_widget.c').read_text()
    finish = block(events, 'boolean ui_widget_port_delete_finish(\n')
    back = block(widgets, 'void ui_widget_port_go_back_past_editor(\n')
    program = r'''
#include <assert.h>
#include <stddef.h>
typedef int boolean;
#define TRUE 1
#define FALSE 0
#define NONE (-1)
struct widget_instance { short local_player_index; };
struct event_record { int unused; };
struct widget_stack_data { long tag; };
struct widget_stack_node { struct widget_stack_data data; struct widget_stack_node *next; };
static struct { struct widget_stack_node *widget_stack[4]; } widget_globals;
static struct { long profile_index; } event_handler_functions;
static long edit_index, shown;
static int deleted, fail_delete;
static void player_ui_end_editing_profile(void) { edit_index = NONE; }
static boolean delete_player_profile_final(struct widget_instance *w, struct event_record *e, boolean *d)
{ assert(edit_index == event_handler_functions.profile_index); if (fail_delete) return FALSE; deleted++; return TRUE; }
static boolean delete_playlist_profile_final(struct widget_instance *w, struct event_record *e, boolean *d)
{ return delete_player_profile_final(w, e, d); }
static void pop_widget(struct widget_stack_node **stack, struct widget_stack_data *data)
{ *data = (*stack)->data; *stack = (*stack)->next; }
static void widget_instance_go_back_to_previous(struct widget_instance *widget)
{ struct widget_stack_data data; pop_widget(&widget_globals.widget_stack[0], &data); shown = data.tag; }
/* BACK */
/* FINISH */
int main(void)
{
    struct widget_instance widget = { 0 };
    for (long type = 0; type <= 1; type++) {
        struct widget_stack_node select = { { 100 }, NULL }, editor = { { 200 }, &select };
        boolean widget_deleted = FALSE;
        widget_globals.widget_stack[0] = &editor;
        edit_index = event_handler_functions.profile_index = 0x80000000L | type;
        fail_delete = 1;
        assert(!ui_widget_port_delete_finish(&widget, &widget_deleted));
        assert(edit_index != NONE && !widget_deleted && widget_globals.widget_stack[0] == &editor);
        fail_delete = 0;
        assert(ui_widget_port_delete_finish(&widget, &widget_deleted));
        assert(edit_index == NONE && event_handler_functions.profile_index == NONE);
        assert(widget_deleted && shown == 100 && widget_globals.widget_stack[0] == NULL);
    }
    assert(deleted == 2);
}
'''
    program = program.replace('/* BACK */', back).replace('/* FINISH */', finish)
    source, binary = tmp_path / 'delete.c', tmp_path / 'delete'
    source.write_text(program)
    subprocess.run(['xcrun', 'clang', '-Wall', '-Werror', '-fsanitize=address,undefined',
                    str(source), '--output=' + str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
