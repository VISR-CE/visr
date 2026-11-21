/* debug.test_input "focus:LABEL@POLL" (test_focus_walk.h): a walk to a row of the list lands on it, and a walk to a label the
list does not have fails, whether the list wraps or stops at its end. */
#include "../../linux/include/test_focus_walk.h"
#include <assert.h>
#include <stdio.h>

/* a list of rows; Down moves the focus to the next, and past the last either wraps or stays */
static int walk(char const *const *rows, int count, int wraps, char const *label, int *final_row)
{
	struct test_focus_walk w;
	int focus = 0, last_down = 0;

	test_focus_walk_begin(&w, label, 10);
	for (unsigned long poll = 0; poll < 2000; poll++)
	{
		int action = test_focus_walk_poll(&w, poll, poll < 50 ? NULL : rows[focus]);

		if (action == TEST_FOCUS_DOWN && !last_down)
			focus = focus + 1 < count ? focus + 1 : wraps ? 0 : focus;
		last_down = action == TEST_FOCUS_DOWN;
		if (action != TEST_FOCUS_DOWN && action != TEST_FOCUS_WAIT)
		{
			*final_row = focus;
			return action;
		}
	}
	*final_row = focus;
	return -1;
}

int main(void)
{
	char const *const rows[] = { "CHANGE NAME", "GAME OPTIONS", "PLAYER OPTIONS", "ITEM OPTIONS", "VEHICLES", "SAVE CHANGES" };
	int row;

	/* a label in the list lands on it, in any case and with a colon on either side */
	assert(walk(rows, 6, 0, "VEHICLES", &row) == TEST_FOCUS_DONE && row == 4);
	assert(walk(rows, 6, 0, "vehicles:", &row) == TEST_FOCUS_DONE && row == 4);
	assert(walk(rows, 6, 1, "SAVE CHANGES", &row) == TEST_FOCUS_DONE && row == 5);
	/* the row it begins on needs no press */
	assert(walk(rows, 6, 0, "CHANGE NAME", &row) == TEST_FOCUS_DONE && row == 0);
	/* the row before the one a count would have named: the label decides, not the count */
	assert(walk(rows + 0, 5, 0, "VEHICLES", &row) == TEST_FOCUS_DONE && row == 4);
	/* a label the list does not have fails: at the list's end, and by wrapping round */
	assert(walk(rows, 6, 0, "DELETE GAMETYPE", &row) == TEST_FOCUS_FAIL_STUCK && row == 5);
	assert(walk(rows, 6, 1, "DELETE GAMETYPE", &row) == TEST_FOCUS_FAIL_WRAP);
	/* no screen with a focused row for too long fails */
	{
		struct test_focus_walk w;
		int action = TEST_FOCUS_WAIT;

		test_focus_walk_begin(&w, "VEHICLES", 0);
		for (unsigned long poll = 0; poll < 5000 && action == TEST_FOCUS_WAIT; poll++)
			action = test_focus_walk_poll(&w, poll, NULL);
		assert(action == TEST_FOCUS_FAIL_NO_SCREEN);
	}
	/* a press scripted before the walk lands fails the walk (the A of "focus:VEHICLES@90 a@150" must not press another row) */
	{
		struct test_focus_walk w;
		int action = TEST_FOCUS_WAIT;

		test_focus_walk_begin(&w, "VEHICLES", 10);
		w.deadline = 60;
		for (unsigned long poll = 0; poll < 200 && action != TEST_FOCUS_FAIL_LATE; poll++)
		{
			char row[16];

			snprintf(row, sizeof(row), "ROW %lu", poll / 8);   /* the focus keeps moving, to rows the label is not */
			action = test_focus_walk_poll(&w, poll, poll < 20 ? NULL : row);
		}
		assert(action == TEST_FOCUS_FAIL_LATE);
	}
	puts("test_focus_walk_probe: ok");
	return 0;
}
