/*
HALO_MENU_FOCUS.H

The menus' focus is a chain of focused_child links from a screen down to one widget. Focus is drawn locally: a list
draws the row its own link names as focused, whether or not the chain reaches the list, so when focus leaves a part of
the screen the links there are cleared (two lists on one screen would otherwise both draw a focused row).

One kind of link is never cleared: the links inside a list item. A list item is a child of a list that is not a list
itself: an option row, whose link names its spinner. The row sends Left and Right to that spinner and draws the
spinner's value by it, so a row whose link is gone ignores its keys and draws its value in the unfocused color (the
menus' blue, which reads as a read-only value). That link is the row's own, not part of where the focus is: every row
of a fresh screen has it, focused or not, and it must survive the focus moving between rows, whichever input moved it.

HALO_MENU_FOCUS_DEFINE(prefix, type, is_list) makes the rule for a node type with parent and focused_child members
and a list test, so the game (ui_widget.c, struct widget_instance) and a probe on stand-in trees run the same code:

  prefix_is_list_item(node)       a child of a list that is not a list itself
  prefix_inside_list_item(node)   the node is a list item or below one
  prefix_clear_chain(first)       clears the links of the chain from first down, as the game's focus change does,
                                  except the links inside list items; the walk still follows them
  prefix_give(root, new_focus)    the game's focus change (widget_instance_give_focus_directly, after its choice of a
                                  stand-in for a disabled widget): a sibling of the root's focused child takes its
                                  place; else the old chain is cleared by prefix_clear_chain and the links from the
                                  new focus up to the root are set
  prefix_item_lost_focus(node)    a list item that sends its events to its focused child (forwards) and has none,
                                  though it has a child that could take them: the state the rule exists to prevent
*/

#ifndef HALO_MENU_FOCUS_H
#define HALO_MENU_FOCUS_H

/* (made in the including file, which need not use every one) */
#if defined(__GNUC__) || defined(__clang__)
#define HALO_MENU_FOCUS_FUNCTION __attribute__((unused)) static inline
#else
#define HALO_MENU_FOCUS_FUNCTION static inline
#endif

#define HALO_MENU_FOCUS_DEFINE(prefix, type, is_list) \
HALO_MENU_FOCUS_FUNCTION int prefix##_is_list_item(type const *node) \
{ \
	return node && node->parent && is_list(node->parent) && !is_list(node); \
} \
HALO_MENU_FOCUS_FUNCTION int prefix##_inside_list_item(type const *node) \
{ \
	for (; node; node = node->parent) \
		if (prefix##_is_list_item(node)) return 1; \
	return 0; \
} \
HALO_MENU_FOCUS_FUNCTION void prefix##_clear_chain(type *first) \
{ \
	while (first) \
	{ \
		type *next = first->focused_child; \
		if (first->parent && !prefix##_inside_list_item(first->parent)) \
			first->parent->focused_child = 0; \
		first = next; \
	} \
} \
HALO_MENU_FOCUS_FUNCTION void prefix##_give(type *root, type *new_focus) \
{ \
	type *focused_child = root->focused_child; \
	if (focused_child) \
	{ \
		if (new_focus && focused_child->parent == new_focus->parent && focused_child->parent) \
		{ \
			new_focus->parent->focused_child = new_focus; \
			return; \
		} \
		prefix##_clear_chain(focused_child); \
	} \
	while (new_focus->parent) \
	{ \
		new_focus->parent->focused_child = new_focus; \
		new_focus = new_focus->parent; \
	} \
} \
HALO_MENU_FOCUS_FUNCTION int prefix##_item_lost_focus(type const *node, int forwards, int has_event_child) \
{ \
	return prefix##_is_list_item(node) && forwards && has_event_child && !node->focused_child; \
}

#endif
