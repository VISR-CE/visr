/* debug.test_input "focus:LABEL@POLL": a menu walk that presses Down until the focused row's text is LABEL, instead of
counting rows (a count is right for one build's menu and wrong for the next one's). This is the walk's decision only; the
caller reads the focused row's text from the same string the widget renders (menu_test_focus_text) and holds Down. */
#ifndef TEST_FOCUS_WALK_H
#define TEST_FOCUS_WALK_H

#include <string.h>

#define TEST_FOCUS_LABEL_MAX 48
#define TEST_FOCUS_HOLD_POLLS 4    /* polls Down is held for a step */
#define TEST_FOCUS_CHECK_POLLS 12  /* polls between looks at the focus: the move has drawn by then */
#define TEST_FOCUS_BLIND_LOOKS 4   /* looks that find no focused row before one Down is pressed to give a page its focus */
#define TEST_FOCUS_AFTER_POLLS 40  /* polls past the deadline of the last check: a capture's frame is taken about then */
#define TEST_FOCUS_WAIT_POLLS 3000 /* polls to wait for a screen with a focused row before the walk fails */
#define TEST_FOCUS_EXIT_STATUS 87  /* the process status of a failed walk where the platform has one */

enum test_focus_action
{
	TEST_FOCUS_WAIT,      /* nothing pressed */
	TEST_FOCUS_DOWN,      /* hold Down this poll */
	TEST_FOCUS_DONE,      /* the focused row is the label */
	TEST_FOCUS_FAIL_STUCK, /* Down no longer moves the focus: the label is not in the list */
	TEST_FOCUS_FAIL_WRAP,  /* the focus came back to where it began: the label is not in the list */
	TEST_FOCUS_FAIL_NO_SCREEN, /* no screen with a focused row came up */
	TEST_FOCUS_FAIL_LATE, /* the next scripted press came before the walk had landed */
	TEST_FOCUS_FAIL_LOST  /* the walk landed and the focus left the row again before its deadline (a menu that redraws its rows) */
};

struct test_focus_walk
{
	char label[TEST_FOCUS_LABEL_MAX];
	unsigned long start;       /* the poll the walk begins at */
	int hold_to_end;           /* the deadline was given ("focus:LABEL@POLL:DEADLINE"): no press follows, so the focus must stay on the row past it */
	unsigned long deadline;    /* the poll of the next scripted press, 0 for none: the walk must have landed by then */
	unsigned long next_look;   /* the poll of the next look at the focus */
	unsigned long hold_until;  /* Down is held until this poll */
	int moved;                 /* Down has been pressed at least once */
	int blind;                 /* looks in a row that found no focused row */
	int unchanged;             /* looks in a row at the same row after a press (one Down can be lost while a screen opens) */
	int finished;              /* the walk has ended, with `result` */
	int result;
	char first[TEST_FOCUS_LABEL_MAX]; /* the row the walk began on */
	char last[TEST_FOCUS_LABEL_MAX];  /* the row at the previous look */
};

static inline char test_focus_fold(char c)
{
	return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
}

/* two row texts are the same when they match without case, ignoring a trailing colon and spaces ("TIME LIMIT:") */
static inline int test_focus_label_equal(char const *a, char const *b)
{
	size_t la = strlen(a), lb = strlen(b);

	while (la && (a[la - 1] == ':' || a[la - 1] == ' ')) la--;
	while (lb && (b[lb - 1] == ':' || b[lb - 1] == ' ')) lb--;
	if (la != lb)
		return 0;
	for (size_t i = 0; i < la; i++)
		if (test_focus_fold(a[i]) != test_focus_fold(b[i]))
			return 0;
	return 1;
}

static inline void test_focus_walk_begin(struct test_focus_walk *walk, char const *label, unsigned long start)
{
	memset(walk, 0, sizeof(*walk));
	strncpy(walk->label, label, TEST_FOCUS_LABEL_MAX - 1);
	walk->start = walk->next_look = start;
}

/* the polls a landed walk checks the focus again at: just before its deadline and where a capture's frame falls after it */
static inline int test_focus_walk_checks_at(struct test_focus_walk const *walk, unsigned long poll)
{
	/* (a press after the walk moves the focus on purpose: only a walk that ends the script is checked past its deadline) */
	return walk->deadline > TEST_FOCUS_CHECK_POLLS &&
		(poll == walk->deadline - 2 || (walk->hold_to_end && poll == walk->deadline + TEST_FOCUS_AFTER_POLLS));
}

/* whether the caller should read the focused row's text this poll: a walk that is looking, or a landed one making its last
check before the deadline (the focus must still be on the row when the next press or the shot comes) */
static inline int test_focus_walk_wants_look(struct test_focus_walk const *walk, unsigned long poll)
{
	if (walk->finished)
		return walk->result == TEST_FOCUS_DONE && test_focus_walk_checks_at(walk, poll);
	return poll >= walk->start && poll >= walk->hold_until && poll >= walk->next_look;
}

/* one poll of the walk. `seen` is the focused row's text, or NULL when no screen has a focused row; it is read only on a
poll that looks. Once the walk has finished it returns its result on every poll. */
static inline int test_focus_walk_poll(struct test_focus_walk *walk, unsigned long poll, char const *seen)
{
	if (walk->finished)
	{
		if (walk->result == TEST_FOCUS_DONE && test_focus_walk_checks_at(walk, poll) && seen &&
			!test_focus_label_equal(seen, walk->label))
		{
			strncpy(walk->last, seen, TEST_FOCUS_LABEL_MAX - 1);
			walk->result = TEST_FOCUS_FAIL_LOST;
		}
		return walk->result;
	}
	if (poll < walk->start)
		return TEST_FOCUS_WAIT;
	if (walk->deadline && poll >= walk->deadline)
	{
		walk->finished = 1;
		walk->result = TEST_FOCUS_FAIL_LATE;
		return walk->result;
	}
	if (poll < walk->hold_until)
		return TEST_FOCUS_DOWN;
	if (poll < walk->next_look)
		return TEST_FOCUS_WAIT;
	walk->next_look = poll + TEST_FOCUS_CHECK_POLLS;
	if (!seen)
	{
		if (poll - walk->start > TEST_FOCUS_WAIT_POLLS)
		{
			walk->finished = 1;
			walk->result = TEST_FOCUS_FAIL_NO_SCREEN;
			return walk->result;
		}
		/* a page that comes up with no row focused (a form page) takes its focus from the first Down */
		if (++walk->blind >= TEST_FOCUS_BLIND_LOOKS)
		{
			walk->blind = 0;
			walk->hold_until = poll + TEST_FOCUS_HOLD_POLLS;
			return TEST_FOCUS_DOWN;
		}
		return TEST_FOCUS_WAIT;
	}
	walk->blind = 0;
	if (test_focus_label_equal(seen, walk->label))
	{
		walk->finished = 1;
		walk->result = TEST_FOCUS_DONE;
		return walk->result;
	}
	if (!walk->moved)
		strncpy(walk->first, seen, TEST_FOCUS_LABEL_MAX - 1);
	else if (!strcmp(seen, walk->last))
	{
		if (++walk->unchanged >= 2)
		{
			walk->finished = 1;
			walk->result = TEST_FOCUS_FAIL_STUCK;
			return walk->result;
		}
	}
	else if (!strcmp(seen, walk->first))
	{
		walk->finished = 1;
		walk->result = TEST_FOCUS_FAIL_WRAP;
		return walk->result;
	}
	if (strcmp(seen, walk->last))
		walk->unchanged = 0;
	strncpy(walk->last, seen, TEST_FOCUS_LABEL_MAX - 1);
	walk->moved = 1;
	walk->hold_until = poll + TEST_FOCUS_HOLD_POLLS;
	return TEST_FOCUS_DOWN;
}

#endif
