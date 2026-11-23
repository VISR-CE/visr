/*
HALO_POINTER_GATE.H

Decides whether a pointer-device motion event is the user moving the mouse. Only that
may change which input the menus follow. A window gaining or losing focus, or the
pointer entering or leaving it, makes the system report the cursor's position again
(a synthetic motion, sometimes with the distance the cursor was warped); that first
report only resynchronizes the position. A motion that leaves the position unchanged
is not movement either.
*/

#ifndef HALO_POINTER_GATE_H
#define HALO_POINTER_GATE_H

struct halo_pointer_gate
{
	int settle;     /* the next motion event only reports where the cursor already is */
	int known;      /* x and y hold a reported position */
	float x, y;
};

/* focus gained or lost, the pointer entered or left the window, the app was activated */
static inline void halo_pointer_gate_window_changed(struct halo_pointer_gate *gate)
{
	gate->settle = 1;
}

/* 1 when this motion event is real movement; always records the position */
static inline int halo_pointer_gate_motion(struct halo_pointer_gate *gate, float x, float y)
{
	int real = !gate->settle && (!gate->known || x != gate->x || y != gate->y);

	gate->settle = 0;
	gate->known = 1;
	gate->x = x;
	gate->y = y;
	return real;
}

#endif
