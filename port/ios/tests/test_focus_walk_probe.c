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

		if (action == TEST_FOCUS_DOWN && !last_down && poll >= 50)   /* (before that no screen is up) */
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

/* One step of a list: Down (+1) or Up (-1), wrapping or stopping at the ends. */
static int step_row(int focus, int direction, int count, int wraps)
{
	int next = focus + direction;

	if (next < 0)
		return wraps ? count - 1 : focus;
	if (next >= count)
		return wraps ? 0 : focus;
	return next;
}

/* The engine repeats a held d-pad: a second poll of the same press, on a frame slow enough, steps again (source/interface/ui_widget.c,
250 ms). The model makes every second consecutive Down poll step again (the slowest case), loses the press with index `lost` (a loading
screen consumes it unseen) and doubles none of the presses otherwise. Returns the walk's result and the row it ended on; `*max_run` is the
longest run of consecutive polls the walk held Down. */
static int sweep_walk(int count, int wraps, int start, char const *label, int lost, int *final_row, int *max_run)
{
	static char const *const names[] = { "R0", "R1", "R2", "R3", "R4", "R5", "R6" };
	struct test_focus_walk w;
	int focus = start, run = 0, press = 0;

	*max_run = 0;
	test_focus_walk_begin(&w, label, 10);
	for (unsigned long poll = 0; poll < 4000; poll++)
	{
		int action = test_focus_walk_poll(&w, poll, poll < 50 ? NULL : names[focus]);

		run = action == TEST_FOCUS_DOWN ? run + 1 : 0;
		if (run > *max_run)
			*max_run = run;
		if (action == TEST_FOCUS_DOWN && poll >= 50)
		{
			if (run == 1 && press++ != lost)
				focus = step_row(focus, 1, count, wraps);
			else if (run > 1)
				focus = step_row(focus, 1, count, wraps);   /* the repeat fired inside the hold */
		}
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
	/* A press is one poll, so the engine's repeat cannot step it twice: for every list length, wrapping or not, every start row and
	every target row, the walk lands on its target, the press never lasts two polls, and one lost press does not end the walk. */
	for (int count = 2; count <= 7; count++)
		for (int wraps = 0; wraps <= 1; wraps++)
			for (int start = 0; start < count; start++)
				for (int target = start; target < count; target++)
					for (int lost = -1; lost < 3; lost++)
					{
						char label[4];
						int final_row, max_run;

						snprintf(label, sizeof(label), "R%d", target);
						assert(sweep_walk(count, wraps, start, label, lost, &final_row, &max_run) == TEST_FOCUS_DONE);
						assert(final_row == target && max_run <= TEST_DPAD_PRESS_POLLS && TEST_DPAD_PRESS_POLLS == 1);
					}
	/* and the walk never holds Down on two consecutive polls while it keeps pressing (a label the list does not have) */
	{
		struct test_focus_walk w;
		int run = 0, longest = 0;

		test_focus_walk_begin(&w, "NOWHERE", 10);
		for (unsigned long poll = 0; poll < 400; poll++)
		{
			run = test_focus_walk_poll(&w, poll, poll < 50 ? NULL : "R0") == TEST_FOCUS_DOWN ? run + 1 : 0;
			if (run > longest)
				longest = run;
		}
		assert(longest == 1);   /* one poll per press, whatever the walk does */
	}
	/* two rows with the same text, or a Down lost once, do not end the walk */
	{
		char const *const twins[] = { "A", "B", "B", "C", "VEHICLES" };

		assert(walk(twins, 5, 0, "VEHICLES", &row) == TEST_FOCUS_DONE && row == 4);
	}
	/* a page with no row focused takes its focus from the first Down, and the walk goes on from there */
	{
		struct test_focus_walk w;
		int focus = -1, last_down = 0, action = TEST_FOCUS_WAIT;
		char const *const page[] = { "SCORE TO WIN:", "TIME LIMIT:", "TEAM PLAY:", "TEAMS & HUD:", "TYPE OPTIONS" };

		test_focus_walk_begin(&w, "TEAMS & HUD", 0);
		for (unsigned long poll = 0; poll < 1000 && action != TEST_FOCUS_DONE; poll++)
		{
			action = test_focus_walk_poll(&w, poll, test_focus_walk_wants_look(&w, poll) ? (focus < 0 ? NULL : page[focus]) : NULL);
			if (action == TEST_FOCUS_DOWN && !last_down && focus < 4)
				focus++;
			last_down = action == TEST_FOCUS_DOWN;
		}
		assert(action == TEST_FOCUS_DONE && focus == 3);
	}
	/* no screen with a focused row for too long fails */
	{
		struct test_focus_walk w;
		int action = TEST_FOCUS_WAIT;

		test_focus_walk_begin(&w, "VEHICLES", 0);
		for (unsigned long poll = 0; poll < 5000 && action != TEST_FOCUS_FAIL_NO_SCREEN; poll++)
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
	/* a walk that landed and then lost the focus before its deadline fails at the last check */
	{
		struct test_focus_walk w;
		int action = TEST_FOCUS_WAIT;

		test_focus_walk_begin(&w, "INTERNET", 10);
		w.check_at = 98;   /* (just before the next scripted press) */
		for (unsigned long poll = 0; poll < 200 && action != TEST_FOCUS_FAIL_LOST; poll++)
		{
			char const *seen = poll < 20 ? NULL : poll < 40 ? "INTERNET" : "COOPERATIVE PLAY";

			action = test_focus_walk_poll(&w, poll, test_focus_walk_wants_look(&w, poll) ? seen : NULL);
			assert(action != TEST_FOCUS_FAIL_NO_SCREEN && action != TEST_FOCUS_FAIL_LATE);
		}
		assert(action == TEST_FOCUS_FAIL_LOST);
		/* and one that keeps the row to the deadline does not */
		test_focus_walk_begin(&w, "INTERNET", 10);
		w.check_at = 98;   /* (just before the next scripted press) */
		for (unsigned long poll = 0; poll < 200 && poll < 99; poll++)
			action = test_focus_walk_poll(&w, poll, test_focus_walk_wants_look(&w, poll) ? (poll < 20 ? NULL : "INTERNET") : NULL);
		assert(action == TEST_FOCUS_DONE);
	}
	/* the check after the deadline, where a capture's frame falls: a focus that drifts off the row then fails too */
	{
		struct test_focus_walk w;
		int action = TEST_FOCUS_WAIT;

		test_focus_walk_begin(&w, "SPLIT SCREEN", 10);
		w.deadline = 100;
		w.hold_to_end = 1;
		for (unsigned long poll = 0; poll < 300 && action != TEST_FOCUS_FAIL_LOST; poll++)
		{
			char const *seen = poll < 20 ? NULL : poll < 120 ? "SPLIT SCREEN" : "COOPERATIVE PLAY";

			action = test_focus_walk_poll(&w, poll, test_focus_walk_wants_look(&w, poll) ? seen : NULL);
		}
		assert(action == TEST_FOCUS_FAIL_LOST);
		/* a walk a press follows (the A that opens the row) is not checked past its deadline: the focus moves on purpose */
		test_focus_walk_begin(&w, "SPLIT SCREEN", 10);
		w.deadline = 100;
		for (unsigned long poll = 0; poll < 300; poll++)
		{
			char const *seen = poll < 20 ? NULL : poll < 120 ? "SPLIT SCREEN" : "NEXT PAGE ROW";

			action = test_focus_walk_poll(&w, poll, test_focus_walk_wants_look(&w, poll) ? seen : NULL);
			assert(action != TEST_FOCUS_FAIL_LOST);
		}
	}
	/* the script's steps are one chain: a slow first walk delays the press after it AND the second walk and its press
	("focus:A@90 a@150 focus:B@210 a@240", the settings hub flow) */
	{
		struct test_chain_step steps[4] = {
			{ .is_walk = 1, .index = 0, .poll = 90 }, { .index = 0, .poll = 150 },
			{ .is_walk = 1, .index = 1, .poll = 210 }, { .index = 1, .poll = 240 } };

		for (unsigned long poll = 0; poll < 400; poll++)
			test_chain_update(steps, 4, poll);
		assert(steps[0].released && steps[0].shift == 0);   /* the first walk starts on time and has not landed */
		assert(!steps[1].released && !steps[2].released && !steps[3].released);
		steps[0].landed = 1;
		steps[0].landed_at = 400;   /* it landed at poll 400, 250 polls after the press that follows it was due */
		test_chain_update(steps, 4, 400);
		assert(steps[1].released && steps[1].shift == 250);
		assert(!steps[2].released);   /* the second walk is due at its own poll plus the delay: 210 + 250 */
		for (unsigned long poll = 401; poll < 600; poll++)
		{
			test_chain_update(steps, 4, poll);
			if (poll == 470 && steps[2].released)   /* the second walk runs 250 polls behind and lands 10 polls after its start */
			{
				steps[2].landed = 1;
				steps[2].landed_at = poll;
			}
		}
		assert(steps[2].released && steps[2].shift == 250 && steps[3].released && steps[3].shift == 250);
		/* a walk that lands in time delays nothing, and a chord stays together */
		struct test_chain_step quick[3] = { { .is_walk = 1, .poll = 10 }, { .index = 0, .poll = 100 }, { .index = 1, .poll = 100 } };
		test_chain_update(quick, 3, 10);
		quick[0].landed = 1;
		quick[0].landed_at = 40;
		for (unsigned long poll = 11; poll <= 100; poll++)
			test_chain_update(quick, 3, poll);
		assert(quick[1].released && quick[1].shift == 0 && quick[2].released && quick[2].shift == 0);
		/* with no walk in the script nothing moves */
		struct test_chain_step plain[2] = { { .poll = 8 }, { .poll = 16 } };
		for (unsigned long poll = 0; poll <= 16; poll++)
			test_chain_update(plain, 2, poll);
		assert(plain[0].shift == 0 && plain[1].shift == 0);
	}

	/* The script's clock begins when a screen is up, the same for every input (pgi-sim-retake: the keyboard and touch runs showed
	their screen at frames 153 and 152, a controller's at 2, a mouse's at 68; a press scripted for poll 90 fired at frame 90 in each,
	before the keyboard's screen existed). Whatever frame the screen comes up at, the press fires 90 polls after it and the polls it
	waited are reported once. */
	{
		struct { char const *input; unsigned long screen_frame; } inputs[] = {
			{ "controller", 2 }, { "mouse", 68 }, { "touch", 152 }, { "keyboard", 153 } };

		for (int i = 0; i < 4; i++)
		{
			struct test_script_clock clock = { 0 };
			unsigned long script_poll = 0, fired = 0;
			int pressed_early = 0;

			for (unsigned long frame = 0; frame < 500; frame++)
			{
				if (!test_script_clock_tick(&clock, frame >= inputs[i].screen_frame, &script_poll))
					continue;
				if (frame < inputs[i].screen_frame)
					pressed_early = 1;
				if (script_poll == 90 && !fired)
					fired = frame;
			}
			assert(!pressed_early && fired == inputs[i].screen_frame + 90 && clock.waited == inputs[i].screen_frame);
		}
	}

	puts("test_focus_walk_probe: ok");
	return 0;
}
