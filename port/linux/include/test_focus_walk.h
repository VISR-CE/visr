/* debug.test_input "focus:LABEL@POLL": a menu walk that presses Down until the focused row's text is LABEL, instead of
counting rows (a count is right for one build's menu and wrong for the next one's). This is the walk's decision only; the
caller reads the focused row's text from the same string the widget renders (menu_test_focus_text) and holds Down. */
#ifndef TEST_FOCUS_WALK_H
#define TEST_FOCUS_WALK_H

#include <string.h>

#define TEST_FOCUS_LABEL_MAX 48
/* A scripted d-pad press is ONE poll. The engine repeats a held d-pad: a button held for a second poll has hold value 2 or more, and
a d-pad event with value above 1 counts as a fresh press once 250 ms of wall clock have passed since the last value-1 event
(source/interface/ui_widget.c, DPAD_EVENT_REPEAT_MILLISECONDS). A press of N polls double-steps whenever its N - 1 frame intervals add
up to 250 ms, and a simulator frame can take that long (a 4-poll hold stepped PROFILES past SETTINGS to LOAD CAMPAIGN). One poll only ever
has value 1, so it cannot repeat at any frame time. The walk confirms each step by looking at the focus, and a lost press is retried. */
#define TEST_DPAD_PRESS_POLLS 1
#define TEST_FOCUS_HOLD_POLLS TEST_DPAD_PRESS_POLLS /* polls Down is held for a step */
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
	unsigned long deadline;    /* "focus:LABEL@POLL:DEADLINE": the poll the walk must have landed by, 0 for none */
	unsigned long check_at;    /* the poll just before the next scripted press, 0 for none: the focus must still be on the row */
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

/* The script's steps (presses and walks) in order, released as one chain: a step is released at its own poll plus the delay
every step before it has cost, and not before the walk before it has landed (a loaded host takes a walk longer than the script's
frame numbers allow, and everything after a slow walk moves with it). `shift` is a released step's delay in polls; a walk runs on
the virtual poll `poll - shift`, so its own checks keep the script's timing. */
struct test_chain_step
{
	int is_walk;
	int index;                 /* into the caller's presses or walks */
	unsigned long poll;        /* the script's poll for the step */
	int released;
	unsigned long shift;
	int reported;              /* the caller has logged the shift */
	int landed;                /* a walk: it has landed, at `landed_at` (the real poll) */
	unsigned long landed_at;
};

/* The script's clock (debug.test_input, every input's scripted presses and walks): its polls count from the first one at which a
screen is up or a game runs, never from launch. The time a build takes to get there differs by input (a keyboard run showed its
screen at frame 153, a controller run at 2), and a press scripted for poll 90 that fires before the screen exists opens nothing.
The polls spent waiting are reported once (`waited`), so the capture tools can add them to the marks they wait for. The
pointer script (HALO_MENU_POINTER_SCRIPT) is the same rule: its frames are counted only while the menus are up. */
struct test_script_clock
{
	unsigned long polls;    /* the script's own poll, counted from the start */
	unsigned long waited;   /* real polls that passed before the start */
	int started;
};

/* one real poll: returns 0 while no screen is up and the script has not begun; else 1 with the script's poll in `*poll` */
static inline int test_script_clock_tick(struct test_script_clock *clock, int screen_ready, unsigned long *poll)
{
	if (!clock->started)
	{
		if (!screen_ready)
		{
			clock->waited++;
			return 0;
		}
		clock->started = 1;
	}
	*poll = clock->polls++;
	return 1;
}

/* Networking must be up before a scripted capture opens a screen: a network pregame screen opened as the first screen (frame 2)
showed "A networking error has occurred", opened after the main menu it showed ENLISTED PLAYERS. One condition for every screen
and every row, not a delay per screen: the transport is initialized and its link is up, or `TEST_NETWORK_WAIT_FRAMES` have passed
(a device with no link at all must not wait forever). */
#define TEST_NETWORK_WAIT_FRAMES 900

static inline int test_network_ready(int transport_up, unsigned long frame)
{
	return transport_up || frame >= TEST_NETWORK_WAIT_FRAMES;
}

/* releases the steps that are due at `poll`; the caller marks a walk landed as soon as it lands. Returns how many were released. */
static inline int test_chain_update(struct test_chain_step *steps, int count, unsigned long poll)
{
	unsigned long shift = 0;
	int walk = -1, released = 0;

	for (int i = 0; i < count; i++)
	{
		if (!steps[i].released)
		{
			unsigned long release;

			if (poll < steps[i].poll + shift)
				break;   /* nothing after it can be due before it */
			if (walk >= 0 && !steps[walk].landed)
				break;   /* it waits for the walk before it */
			release = steps[i].poll + shift;
			if (walk >= 0 && steps[walk].landed_at > release)
				release = steps[walk].landed_at;
			steps[i].shift = release - steps[i].poll;
			steps[i].released = 1;
			released++;
		}
		shift = steps[i].shift;
		if (steps[i].is_walk)
			walk = i;
	}
	return released;
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
	return (walk->check_at && poll == walk->check_at) ||
		(walk->hold_to_end && walk->deadline > TEST_FOCUS_CHECK_POLLS &&
		(poll == walk->deadline - 2 || poll == walk->deadline + TEST_FOCUS_AFTER_POLLS));
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
