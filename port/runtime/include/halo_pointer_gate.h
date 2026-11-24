/*
HALO_POINTER_GATE.H

One decision per pointer-device motion event: is it the user moving the mouse? Only
that may change which input the menus follow. A window gaining or losing focus, or
the pointer entering or leaving it, makes the system report the cursor's position
again (a synthetic motion, sometimes carrying the distance the cursor was warped);
that first report only resynchronizes the position, and so does a report that leaves
the position unchanged and carries no delta. The host classifies each event once
(host_sdl.c) and every consumer reads the result: a motion that is not real arrives
with zero deltas and the menu pointer is told to take only its position.

Touch and pen motion SDL synthesizes as mouse events never reaches this gate; they
are neither the mouse nor a reason to forget that a position report is pending.
*/

#ifndef HALO_POINTER_GATE_H
#define HALO_POINTER_GATE_H

#include <SDL3/SDL.h>

/* The mouse ID tells nothing here: outside relative mode SDL rewrites every pointer event's ID to its global ID
(SDL_mouse.c, "all mouse events are global mouse events"), real movement and the system's position reports alike. */

struct halo_pointer_gate
{
	int settle;     /* the next mouse motion only reports where the cursor already is */
	int known;      /* x and y hold a reported position */
	float x, y;
};

/* Classifies one event. Returns 1 for a motion that is real mouse movement. Any other motion from a mouse
device is made inert (zero deltas) and returns 0; focus and pointer-enter/leave events arm the settle rule and
return 0. */
static inline int halo_pointer_gate_classify(struct halo_pointer_gate *gate, SDL_Event *event)
{
	int real;

	switch (event->type)
	{
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_FOCUS_LOST:
	case SDL_EVENT_WINDOW_MOUSE_ENTER:
	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
		gate->settle = 1;
		return 0;
	case SDL_EVENT_MOUSE_MOTION:
		break;
	default:
		return 0;
	}
	if (event->motion.which == SDL_TOUCH_MOUSEID || event->motion.which == SDL_PEN_MOUSEID) return 0;
	real = !gate->settle && (!gate->known || event->motion.x != gate->x || event->motion.y != gate->y ||
		event->motion.xrel != 0.0f || event->motion.yrel != 0.0f);
	gate->settle = 0;
	gate->known = 1;
	gate->x = event->motion.x;
	gate->y = event->motion.y;
	if (!real) event->motion.xrel = event->motion.yrel = 0.0f;
	return real;
}

#endif
