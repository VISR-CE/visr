/*
HALO_UI_DRAG.H

A finger dragging a strip of items along one axis: the map grid's rows and the
carousels' cards. The strip follows the finger once it has moved past the tap
slop, keeps the finger's speed after the lift, and settles on a whole item.
A press that never passes the slop is a tap, which activates the item under it.

Pure arithmetic, shared by the iOS host (host_menu_pointer.h takes its tap
slop from here) and the guest's menus, which own the strips. Positions and
offsets are in whatever units the caller uses (menu units in the guest), times
in seconds.

The offset is how far the strip has scrolled from its start: dragging the
finger toward the start of the axis (up, or left) raises it.
*/

#ifndef HALO_UI_DRAG_H
#define HALO_UI_DRAG_H

#include <math.h>

/* how far a finger may travel, in points on the surface, and still be a tap */
#define HALO_UI_DRAG_SLOP_POINTS 10.0f
/* how quickly a coasting strip slows: its speed falls by e every this many seconds */
#define HALO_UI_DRAG_COAST_SECONDS 0.325f
/* how quickly a strip settles on its item once released: the remaining distance falls by e every this many seconds */
#define HALO_UI_DRAG_SETTLE_SECONDS 0.08f
/* how far past either end the finger can pull the strip, as a fraction of the finger's travel there */
#define HALO_UI_DRAG_EDGE_RESISTANCE 0.35f

enum halo_ui_drag_state
{
	HALO_UI_DRAG_IDLE = 0,
	/* a finger is down and has not passed the slop */
	HALO_UI_DRAG_PRESSED,
	/* a finger is down and moves the strip */
	HALO_UI_DRAG_DRAGGING,
	/* the finger has lifted and the strip is moving to its item */
	HALO_UI_DRAG_SETTLING
};

struct halo_ui_drag
{
	enum halo_ui_drag_state state;
	/* the finger at the press, along and across the axis, and its latest position along it */
	float press_along, press_across, last_along;
	/* the strip's offset at the press, now, and where it will come to rest */
	float origin, offset, target;
	/* the finger's speed along the axis, smoothed, in units per second (positive raises the offset) */
	float velocity;
};

/* past the slop: a drag rather than a tap */
static inline int halo_ui_drag_past_slop(float dx, float dy, float slop)
{
	return dx * dx + dy * dy > slop * slop;
}

/* an offset pulled past [minimum, maximum] gives way less than the finger moves */
static inline float halo_ui_drag_resist(float offset, float minimum, float maximum)
{
	if (offset < minimum)
		return minimum - (minimum - offset) * HALO_UI_DRAG_EDGE_RESISTANCE;
	if (offset > maximum)
		return maximum + (offset - maximum) * HALO_UI_DRAG_EDGE_RESISTANCE;
	return offset;
}

/* the whole item nearest offset, every pitch units, inside [minimum, maximum] */
static inline float halo_ui_drag_snap(float offset, float pitch, float minimum, float maximum)
{
	float snapped = pitch > 0.0f ? roundf(offset / pitch) * pitch : offset;

	return snapped < minimum ? minimum : snapped > maximum ? maximum : snapped;
}

static inline void halo_ui_drag_reset(struct halo_ui_drag *d, float offset)
{
	d->state = HALO_UI_DRAG_IDLE;
	d->press_along = d->press_across = d->last_along = 0.0f;
	d->origin = d->offset = d->target = offset;
	d->velocity = 0.0f;
}

/* a finger came down; a settling strip stops where it is, so a press catches it */
static inline void halo_ui_drag_press(struct halo_ui_drag *d, float along, float across)
{
	d->state = HALO_UI_DRAG_PRESSED;
	d->press_along = d->last_along = along;
	d->press_across = across;
	d->origin = d->target = d->offset;
	d->velocity = 0.0f;
}

/* the finger moved, dt seconds after its last report; returns 1 while it drags the strip */
static inline int halo_ui_drag_move(struct halo_ui_drag *d, float along, float across, float slop, float dt,
	float minimum, float maximum)
{
	float delta;

	if (d->state == HALO_UI_DRAG_PRESSED &&
		halo_ui_drag_past_slop(along - d->press_along, across - d->press_across, slop))
	{
		d->state = HALO_UI_DRAG_DRAGGING;
		/* (the strip starts from where the finger is, not jumping by the slop) */
		d->press_along = d->last_along = along;
	}
	if (d->state != HALO_UI_DRAG_DRAGGING)
	{
		d->last_along = along;
		return 0;
	}
	delta = d->last_along - along;
	if (dt > 0.0f)
	{
		float instant = delta / dt;
		/* (the latest motion counts most: about the last 50 ms) */
		float weight = dt / (dt + 0.05f);

		d->velocity += (instant - d->velocity) * weight;
	}
	d->last_along = along;
	d->offset = halo_ui_drag_resist(d->origin + (d->press_along - along), minimum, maximum);
	return 1;
}

/* the finger lifted: returns 1 for a tap (it never passed the slop); otherwise
the strip carries on at the finger's speed and settles on the item every pitch
units nearest where that speed would have brought it to rest */
static inline int halo_ui_drag_release(struct halo_ui_drag *d, float pitch, float minimum, float maximum)
{
	int tap = d->state == HALO_UI_DRAG_PRESSED;

	if (d->state == HALO_UI_DRAG_IDLE)
		return 0;
	d->target = halo_ui_drag_snap(d->offset + d->velocity * HALO_UI_DRAG_COAST_SECONDS, pitch, minimum, maximum);
	d->state = HALO_UI_DRAG_SETTLING;
	d->velocity = 0.0f;
	return tap;
}

/* the finger went away without a lift (the menus closed, the touch was
canceled): settle on the nearest item, with no tap */
static inline void halo_ui_drag_cancel(struct halo_ui_drag *d, float pitch, float minimum, float maximum)
{
	if (d->state == HALO_UI_DRAG_IDLE)
		return;
	d->target = halo_ui_drag_snap(d->offset, pitch, minimum, maximum);
	d->state = HALO_UI_DRAG_SETTLING;
	d->velocity = 0.0f;
}

/* move a settling strip dt seconds toward its target; returns 1 while it is still moving */
static inline int halo_ui_drag_step(struct halo_ui_drag *d, float dt)
{
	if (d->state != HALO_UI_DRAG_SETTLING)
		return d->state != HALO_UI_DRAG_IDLE;
	if (dt > 0.0f)
		d->offset = d->target + (d->offset - d->target) * expf(-dt / HALO_UI_DRAG_SETTLE_SECONDS);
	if (fabsf(d->offset - d->target) < 0.25f)
	{
		d->offset = d->target;
		d->state = HALO_UI_DRAG_IDLE;
		return 0;
	}
	return 1;
}

/* the controller or a wheel moved the strip to another item: glide there */
static inline void halo_ui_drag_glide(struct halo_ui_drag *d, float target)
{
	if (d->state == HALO_UI_DRAG_PRESSED || d->state == HALO_UI_DRAG_DRAGGING)
		return;
	d->target = target;
	d->state = HALO_UI_DRAG_SETTLING;
}

/* The host's wheel steps are positive down the screen (a finger dragging up, a wheel turned toward the user); the
menus' are positive up the list, clamped to 8. */
static inline int halo_ui_wheel_steps_from_host(int host_steps)
{
	return host_steps < -8 ? 8 : host_steps > 8 ? -8 : -host_steps;
}

/* A list of rows that moves by the menus' wheel steps (positive up the list). Returns how far to move the highlight
toward the list's end (positive) or start (negative). A wheel moves it as the d-pad would: up is back. A finger
dragging down makes positive steps and the highlight goes down with it, so the sign flips; a finger over a row leaves
the highlight to the pointer (hover), so it moves nothing until the finger is past the rows. (A dropdown's panel moves
its content with the finger and takes the wheel's sign: -steps for both.) */
static inline int halo_ui_list_wheel_steps(int wheel_steps, int finger, int over_row)
{
	if (!finger)
		return -wheel_steps;
	return over_row ? 0 : wheel_steps;
}

#endif
