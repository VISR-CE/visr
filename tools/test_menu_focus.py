"""The menus' focus rule (halo_menu_focus.h): no focus change takes a list item's own focus.

Bug class: a focus change that clears links inside a list item. An option row sends Left and Right to its focused
child, the spinner, and draws the spinner's value by it; the game's focus change cleared that link in every row the
focus moved off, so after the mouse crossed a settings screen the rows ignored their keys and drew their values in
the unfocused blue, which reads as read-only. The probe runs the game's own focus change (the header's, which
ui_widget.c calls) over every order of focus moves on a screen with two lists of rows, and runs the game's original
loop beside it to show the check catches the class.
"""
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

PROBE = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "halo_menu_focus.h"

struct node { struct node *parent, *focused_child; int list; };
#define IS_LIST(node) ((node)->list)
#define NEVER_LIST(node) ((void)(node), 0)
HALO_MENU_FOCUS_DEFINE(focus, struct node, IS_LIST)
/* the game's original loop: with no list items, every link of the old chain is cleared */
HALO_MENU_FOCUS_DEFINE(original, struct node, NEVER_LIST)

/* a screen with two lists (as the server browser's list and detail panel), each of three rows; a row holds a label
and a spinner (a list of its values), and one row holds two spinners */
enum { ROWS = 3 };
static struct node screen, lists[2], rows[2][ROWS], labels[2][ROWS], spinners[2][ROWS], seconds[2];
static struct node *targets[32];
static int target_count;

static void build(void)
{
	memset(&screen, 0, sizeof(screen));
	memset(lists, 0, sizeof(lists)); memset(rows, 0, sizeof(rows)); memset(labels, 0, sizeof(labels));
	memset(spinners, 0, sizeof(spinners)); memset(seconds, 0, sizeof(seconds));
	target_count = 0;
	for (int l = 0; l < 2; l++)
	{
		lists[l].parent = &screen; lists[l].list = 1;
		seconds[l].parent = &rows[l][ROWS - 1]; seconds[l].list = 1;
		for (int r = 0; r < ROWS; r++)
		{
			rows[l][r].parent = &lists[l];
			labels[l][r].parent = spinners[l][r].parent = &rows[l][r];
			spinners[l][r].list = 1;
			/* a fresh screen: every row focuses its spinner (widget creation's focus_a_child) */
			rows[l][r].focused_child = &spinners[l][r];
			targets[target_count++] = &rows[l][r];      /* the pointer on a row, a list's item step */
			targets[target_count++] = &spinners[l][r];  /* the pointer on a row's value */
		}
		targets[target_count++] = &seconds[l];          /* the second spinner of a row with two */
	}
	lists[0].focused_child = &rows[0][0];
	screen.focused_child = &lists[0];
}

/* what must hold after any focus change: every row still has a focused child, and only the list the focus is in
draws a focused row (the other's link is cleared) */
static int rows_kept(void)
{
	for (int l = 0; l < 2; l++)
		for (int r = 0; r < ROWS; r++)
			if (!rows[l][r].focused_child || focus_item_lost_focus(&rows[l][r], 1, 1)) return 0;
	return 1;
}

static int one_list_focused(void)
{
	return screen.focused_child && !(lists[0].focused_child && lists[1].focused_child) &&
		screen.focused_child->focused_child;
}

static long walk(int depth, int original, long *broken)
{
	long count = 0;

	if (!depth)
		return 1;
	for (int index = 0; index < target_count; index++)
	{
		struct node saved_screen = screen, saved_lists[2], saved_rows[2][ROWS];

		memcpy(saved_lists, lists, sizeof(lists)); memcpy(saved_rows, rows, sizeof(rows));
		if (original) original_give(&screen, targets[index]); else focus_give(&screen, targets[index]);
		/* the new focus is reached by the chain from the screen */
		{
			struct node *end = &screen;
			while (end->focused_child && end != targets[index]) end = end->focused_child;
			assert(end == targets[index]);
		}
		assert(one_list_focused());
		if (!rows_kept()) ++*broken;
		count += walk(depth - 1, original, broken);
		screen = saved_screen; memcpy(lists, saved_lists, sizeof(lists)); memcpy(rows, saved_rows, sizeof(rows));
	}
	return count;
}

int main(void)
{
	long broken = 0, orders;

	build();
	assert(focus_is_list_item(&rows[0][0]) && !focus_is_list_item(&lists[0]) && !focus_is_list_item(&spinners[0][0]));
	assert(focus_inside_list_item(&spinners[0][0]) && focus_inside_list_item(&rows[1][2]) &&
		!focus_inside_list_item(&lists[1]) && !focus_inside_list_item(&screen));
	/* the bug as it was seen: the pointer crosses the rows of a settings screen */
	focus_give(&screen, &rows[0][1]); focus_give(&screen, &spinners[0][2]); focus_give(&screen, &rows[0][0]);
	assert(rows[0][0].focused_child == &spinners[0][0] && rows[0][1].focused_child == &spinners[0][1] &&
		rows[0][2].focused_child == &spinners[0][2] && lists[0].focused_child == &rows[0][0]);
	/* the focus leaves a list for the other: the first list draws no focused row, its rows keep their spinners */
	focus_give(&screen, &rows[1][1]);
	assert(!lists[0].focused_child && lists[1].focused_child == &rows[1][1] && rows_kept());
	/* a row with two spinners keeps the one chosen while the focus is elsewhere */
	focus_give(&screen, &seconds[0]); focus_give(&screen, &rows[1][0]);
	assert(rows[0][2].focused_child == &seconds[0]);
	/* the state the game logs: a row that forwards its events, has a child to take them, and no focus */
	rows[0][0].focused_child = NULL;
	assert(focus_item_lost_focus(&rows[0][0], 1, 1) && !focus_item_lost_focus(&rows[0][0], 0, 1) &&
		!focus_item_lost_focus(&rows[0][0], 1, 0) && !focus_item_lost_focus(&lists[0], 1, 1));
	/* every order of four focus changes among every target */
	build();
	orders = walk(4, 0, &broken);
	printf("rule: %ld orders, %ld left a row without its focus\n", orders, broken);
	assert(!broken);
	/* the game's original loop over the same orders does break rows: the check sees the class */
	build();
	broken = 0;
	walk(4, 1, &broken);
	printf("original: %ld left a row without its focus\n", broken);
	assert(broken > 0);
	return 0;
}
'''


def test_no_focus_change_takes_a_rows_own_focus(tmp_path):
    probe, binary = tmp_path / 'focus.c', tmp_path / 'focus'
    probe.write_text(PROBE)
    subprocess.run([shutil.which('cc'), '-std=c11', '-Wall', '-Werror', '-iquote', str(ROOT / 'port/linux/include'),
                    str(probe), '-o', str(binary)], check=True)
    output = subprocess.run([str(binary)], check=True, capture_output=True, text=True).stdout
    assert re.search(r'rule: \d+ orders, 0 left', output), output


def test_the_game_changes_focus_through_the_shared_rule():
    """ui_widget.c's one focus change is the header's, so the probe above runs the game's code, and no other place
    in it clears a focus link by hand."""
    source = (ROOT / 'source/interface/ui_widget.c').read_text()
    start = source.index('static void widget_instance_give_focus_directly(\n\tstruct widget_instance *widget,\n'
                         '\tstruct widget_instance *new_focus)\n{')
    body = source[start:source.index('\n}\n', start)]
    assert 'ui_focus_give(widget_instance_get_topmost_parent(widget), new_focus);' in body
    assert 'focused_child = NULL' not in body
    assert not re.search(r'->focused_child\s*=\s*(NULL|0)\s*;', source), \
        'a focus link cleared outside halo_menu_focus.h'
