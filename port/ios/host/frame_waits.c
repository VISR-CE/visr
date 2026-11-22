/* A frame's waits (frame_waits.h). */
#include "frame_waits.h"

static double game_waited, display_waited, slot_waited;
/* the frame's slot wait's start and end (0: none this frame) */
static double slot_started, slot_ended;

void frame_waits_add_game(double seconds)
{
	game_waited += seconds;
}

void frame_waits_add_display(double seconds)
{
	display_waited += seconds;
}

void frame_waits_add_slot(double started, double ended)
{
	slot_started = started;
	slot_ended = ended;
	slot_waited += ended - started;
}

/* the slot wait that was not the frame's work: all of it when the game thread encoded the frame itself; with the
encode thread only the part after the game thread reached its present, where it waited at the barrier for the slot as
it waits for the display */
static double slot_counted(double present_called)
{
	double from;

	if (present_called < 0.0)
		return slot_waited;
	if (slot_ended <= 0.0)
		return 0.0;
	from = slot_started > present_called ? slot_started : present_called;
	return slot_ended > from ? slot_ended - from : 0.0;
}

double frame_waits_display(double present_called)
{
	return display_waited + slot_counted(present_called);
}

struct frame_waits_taken frame_waits_take(double present_called)
{
	struct frame_waits_taken taken;
	double slot = slot_counted(present_called);

	taken.cpu = game_waited + display_waited + slot;
	taken.display = display_waited + slot;
	game_waited = display_waited = slot_waited = slot_started = slot_ended = 0.0;
	return taken;
}
