/* The frame pacer's planner: what the pacer decides, with no clock, no
threads and no graphics, so a probe can drive it with made-up times
(port/ios/tests/frame_pacer_probe.c). host_frame_pacer.c is the pacer
itself: it feeds these the display's refreshes, the settings and the times
frames were shown, and does the waiting.

Three separate things, after innerhat-dev/OpenCE's native Metal host
(test-v0.3.0-net28-mac13, port/macos/host/host_metal.mm: present_acquire and
present_schedule, present_drawable, host_metal_pace_vblank; CC0):

Frames in flight. A frame is in flight from the moment its Present asks for
somewhere to show it until the display shows it. A renderer left to itself
queues finished frames behind the one on screen (a CAMetalLayer has three
drawables, so two deep): each queued frame is a refresh more between the
game drawing something and the player seeing it, and when the queue runs dry
the display repeats a frame. With a limit, a frame waits for one of that many
slots, given back as frames are shown. A limit of 1 (innerhat's) starts
nothing toward the display until the frame before has been shown: the least
delay, at the cost of frames whenever a frame's CPU and GPU work together
take longer than a refresh, since the CPU may not prepare a frame while the
GPU finishes the last. 2 allows exactly that overlap and no more.

A minimum time on screen. A frame is presented to stay up until at least
half a refresh short of its share of the display's time, so one that
finishes early can't replace the frame before it too soon: its share is one
refresh, or with a cap under the display's rate, the cap's (two refreshes
for 60 on a 120 Hz display). Only for frames that wait for the display:
drawing faster than the display, it would itself be a cap.

The game's 60 Hz vertical blank following the display. A timer of its own
drifts against the refresh, and the frames the blank paces (interpolation
off) then bunch, two in one refresh and none in the next. A blank is counted
on each refresh that comes a sixtieth of a second or more after the last one
counted: every refresh at 60 Hz, every other at 120. */
#pragma once

#include <math.h>
#include <string.h>

/* ---------- frames in flight */

enum { FRAME_PACER_IN_FLIGHT_MAXIMUM = 3 };

/* frames in flight when display.frames_in_flight is empty (port/ios/README.md has the measurements behind two): one
with the encode thread (debug.encode_thread = "thread"). Its game thread starts the next frame while the encode
thread finishes this one, so on a GPU-bound device a second frame in flight is a frame of queue between the controller
and the screen: on the A10X iPad Pro at 128 players, two frames in flight showed gameplay 25 to 35 ms later than the
queue off, and one showed it no later, with more frames on time. */
static inline int frame_pacer_in_flight_default(int encode_thread)
{
	return encode_thread ? 1 : 2;
}

/* display.frames_in_flight: "1" to "3"; "system" is 0, no limit of the
pacer's (the renderer's own queue); anything else, fallback */
static inline int frame_pacer_in_flight_setting(const char *value, int fallback)
{
	if (!strcmp(value, "system"))
		return 0;
	if (value[0] >= '1' && value[0] <= '0' + FRAME_PACER_IN_FLIGHT_MAXIMUM && !value[1])
		return value[0] - '0';
	return fallback;
}

/* The frames in flight a frame starts under: display.frames_in_flight's limit (base; 0 is the system's), or while the
frame rate in force is 30 or lower (target, 0 for none) the lower of that and display.frames_in_flight_at_30's
(at_30, 1 or 2; 0 leaves base). It never goes above base, which sized the renderer's buffers when the pacer
started, and a base of 0 (no limit) keeps 0 and its own queue: the pacer's slots do not exist to resize. */
static inline int frame_pacer_in_flight_for(double target, int base, int at_30)
{
	if (!base || target <= 0.0 || target > 30.5)
		return base;
	return at_30 > 0 && at_30 < base ? at_30 : base;
}

struct frame_pacer_flight
{
	int limit; /* 0: none */
	int count; /* frames holding a slot */
};

/* whether a frame may start toward the display now: 1 and it holds a slot,
or 0 and it must wait for frame_pacer_flight_give. Without a limit, 1 and no
slot is held (see frame_pacer_flight_holds). */
static inline int frame_pacer_flight_take(struct frame_pacer_flight *flight)
{
	if (!flight->limit)
		return 1;
	if (flight->count >= flight->limit)
		return 0;
	flight->count++;
	return 1;
}

/* whether a frame that took (frame_pacer_flight_take returned 1) holds a
slot it must give back */
static inline int frame_pacer_flight_holds(const struct frame_pacer_flight *flight)
{
	return flight->limit != 0;
}

/* a frame holding a slot was shown, or had nothing to show */
static inline void frame_pacer_flight_give(struct frame_pacer_flight *flight)
{
	if (flight->count > 0)
		flight->count--;
}

/* When every slot is taken, a frame waits for one. On a Mac the window
server can hold the frame on screen for a tenth of a second at a time (a
trace of a network game showed one about every 5 seconds, its frames held
96 ms with the game's CPU and GPU idle), and waiting the whole hold stops
the game loop with it: input, the game's ticks and the network's messages.
There a frame waits its share of the display plus one refresh of grace
and 1 ms for callback delivery, then gives up its present. A 120 Hz display
link does not guarantee 120 Hz presentation: the compositor may deliver
ordinary frames at 60 Hz even without a frame-rate cap. Timing out after
one refresh would discard a present between almost every pair shown. The
grace allows that cadence while keeping a 96 ms hold out of the loop, so
the loop keeps going and the window keeps the frame it has. Elsewhere, and
once frames have been giving up for limit seconds (a presented notice that
never comes must not keep every frame off the screen), a frame waits up to
limit and then goes ahead without a slot, as before.

drops: whether frames may give up (the Mac); period: the refresh period in
seconds (0 unknown); share: the frame's share of the display in refreshes
(frame_pacer_share); backlog: how long frames have been finding every slot
taken, in seconds (0: this is the first); limit: the longest wait. Returns
the most this frame may wait, in seconds. */
static inline double frame_pacer_slot_wait(int drops, double period, double share, double backlog, double limit)
{
	double turn;

	if (!drops || backlog >= limit)
		return limit;
	turn = (period >= 1.0 / 240.0 && period <= 1.0 / 20.0 ? period : 1.0 / 60.0) *
		((share >= 1.0 ? share : 1.0) + 1.0) + 0.001;
	return turn < limit - backlog ? turn : limit - backlog;
}

/* a frame's wait for a slot ran out (backlog now counts its wait too):
1 to give up its present, 0 to go ahead without a slot */
static inline int frame_pacer_slot_give_up(int drops, double backlog, double limit)
{
	return drops && backlog < limit;
}

/* ---------- a minimum time on screen */

/* how many refreshes a frame's share of the display is: one, or under a
cap (frames a second; 0 none) below the display's rate, the cap's */
static inline double frame_pacer_share(double period, double cap)
{
	double refreshes = 1.0;

	if (cap > 0.0 && period > 0.0)
	{
		refreshes = (double)(long)(1.0 / (cap * period) + 0.5);
		if (refreshes < 1.0)
			refreshes = 1.0;
	}
	return refreshes;
}

/* how long a frame must stay up after the one before it, in seconds; 0: no
minimum (spacing is off, the frame doesn't wait for the display, or the
refresh period, in seconds, isn't a display's) */
static inline double frame_pacer_minimum_duration(int spacing, int waits_for_display, double period, double cap)
{
	if (!spacing || !waits_for_display || period < 1.0 / 240.0 || period > 1.0 / 20.0)
		return 0.0;
	return (frame_pacer_share(period, cap) - 0.5) * period;
}

/* the same where the panel refreshes faster than the display link that gives period (an iPhone, iPad or Apple TV
held under its screen's rate by display.frame_rate: the link runs at the cap, a ProMotion panel at up to 120 Hz):
half a panel refresh short of the frame's share, not half a link period, or a frame held for one 60 Hz period
shows after 8.3 ms on a 120 Hz panel. panel is the panel's refresh period, 0 for unknown */
static inline double frame_pacer_minimum_duration_panel(int spacing, int waits_for_display, double period, double cap,
	double panel)
{
	double duration = frame_pacer_minimum_duration(spacing, waits_for_display, period, cap);

	if (duration > 0.0 && panel > 0.0 && panel < period)
		duration += 0.5 * (period - panel);
	return duration;
}

/* ---------- the 60 Hz vertical blank */

struct frame_pacer_blank
{
	double next;         /* the earliest refresh that counts as the next blank; 0: none seen yet */
	unsigned long ticks; /* blanks counted */
};

#define FRAME_PACER_BLANK_PERIOD (1.0 / 60.0)

/* a refresh of the display at this time (seconds, any steady clock):
whether it counts as a blank */
static inline int frame_pacer_blank_refresh(struct frame_pacer_blank *blank, double refresh)
{
	if (blank->next == 0.0)
		blank->next = refresh;
	/* (a millisecond's grace: a 60 Hz display's refreshes come a hair early
	or late) */
	if (refresh + 0.001 < blank->next)
		return 0;
	blank->ticks++;
	blank->next += FRAME_PACER_BLANK_PERIOD;
	/* the display stopped for a while, or is slower than 60 Hz: start again
	from this refresh instead of counting the missed ones */
	if (refresh > blank->next)
		blank->next = refresh + FRAME_PACER_BLANK_PERIOD;
	return 1;
}

/* ---------- a late start

The delay a player feels is from the moment the game reads the controller to
the moment the picture made from it is on screen. Left alone, the game starts
a frame's work as soon as the last one is handed over, finishes early, and the
finished picture then waits for the display: the wait is at the wrong end.
Starting late moves it to the front: the frame's work begins just early
enough to be done for the next refresh, so the controller is read as late as
it can be. (Nvidia's Reflex and the Vision Pro's Compositor do the same.)

Just early enough is a guess, and a wrong one costs a whole refresh, which is
worse than the wait it saves. So the guess is cautious:

- a frame's cost is the worst of the last FRAME_PACER_LATE_WINDOW frames'
  (its work from the start to the GPU's end), not their average;
- on top of it goes a lead, the time the system needs between a finished
  frame and the refresh that shows it, learned: it starts at
  FRAME_PACER_LATE_LEAD, grows by FRAME_PACER_LATE_LEAD_STEP each time a
  frame is shown later than it aimed for, and shrinks slowly while none is;
- if cost and lead together take more than FRAME_PACER_LATE_FIT of a
  refresh, there is no late start: the frame starts at once, as before;
- a frame that costs more than was allowed for (a fight begins), or is shown
  late, stops late starts for FRAME_PACER_LATE_HOLDOFF frames, until the
  window holds only the new costs. */

enum
{
	FRAME_PACER_LATE_WINDOW = 32,
	FRAME_PACER_LATE_HISTORY = 8,     /* frames seen before the first late start */
	FRAME_PACER_LATE_HOLDOFF = 120
};

#define FRAME_PACER_LATE_LEAD 0.003
#define FRAME_PACER_LATE_LEAD_STEP 0.002
#define FRAME_PACER_LATE_LEAD_MINIMUM 0.002
#define FRAME_PACER_LATE_FIT 0.75
/* frames shown on time before the lead shrinks by a quarter of a step */
#define FRAME_PACER_LATE_CALM 600

struct frame_pacer_late
{
	double costs[FRAME_PACER_LATE_WINDOW];
	int count, next;
	double lead;            /* 0: FRAME_PACER_LATE_LEAD */
	double allowed;         /* the cost the last late start allowed for; 0: it started at once */
	int holdoff;            /* frames still to start at once */
	unsigned long calm;     /* frames shown on time since the lead last changed */
	unsigned long late_starts, at_once, jumps, misses;
};

static inline double frame_pacer_late_worst(const struct frame_pacer_late *late)
{
	double worst = 0.0;
	int index;

	for (index = 0; index < late->count; index++)
		if (late->costs[index] > worst)
			worst = late->costs[index];
	return worst;
}

static inline double frame_pacer_late_lead(const struct frame_pacer_late *late)
{
	return late->lead > 0.0 ? late->lead : FRAME_PACER_LATE_LEAD;
}

/* How long to wait before starting a frame's work, in seconds (0: start at
once), and through aim, the refresh the frame then aims to be shown at (0:
none). now: the clock; refresh: the time of any one refresh of the display
on that clock, and period the time between them, both from the display link. */
static inline double frame_pacer_late_wait(struct frame_pacer_late *late, double now, double refresh, double period,
	double *aim)
{
	double budget, start;

	*aim = 0.0;
	late->allowed = 0.0;
	if (late->holdoff > 0)
	{
		late->holdoff--;
		late->at_once++;
		return 0.0;
	}
	if (late->count < FRAME_PACER_LATE_HISTORY || period < 1.0 / 240.0 || period > 1.0 / 20.0)
	{
		late->at_once++;
		return 0.0;
	}
	budget = frame_pacer_late_worst(late) + frame_pacer_late_lead(late);
	if (budget > FRAME_PACER_LATE_FIT * period)
	{
		late->at_once++;
		return 0.0;
	}
	/* the first refresh the frame can still make if it starts now */
	*aim = refresh + period * (double)(long)((now + budget - refresh) / period + 1.0);
	start = *aim - budget;
	late->allowed = budget - frame_pacer_late_lead(late);
	if (start <= now)
	{
		late->at_once++;
		return 0.0;
	}
	late->late_starts++;
	return start - now;
}

/* a frame's work took this long, from its start to the GPU's end */
static inline void frame_pacer_late_cost(struct frame_pacer_late *late, double cost)
{
	/* (a quarter second or more is a load or the background, not a frame) */
	if (cost <= 0.0 || cost >= 0.25)
		return;
	late->costs[late->next] = cost;
	late->next = (late->next + 1) % FRAME_PACER_LATE_WINDOW;
	if (late->count < FRAME_PACER_LATE_WINDOW)
		late->count++;
	if (late->allowed > 0.0 && cost > late->allowed)
	{
		late->jumps++;
		late->holdoff = FRAME_PACER_LATE_HOLDOFF;
	}
}

/* a frame that aimed for a refresh was shown at this time */
static inline void frame_pacer_late_shown(struct frame_pacer_late *late, double aim, double at, double period)
{
	if (aim <= 0.0 || at <= 0.0)
		return;
	if (at > aim + 0.5 * period)
	{
		late->misses++;
		late->holdoff = FRAME_PACER_LATE_HOLDOFF;
		late->lead = frame_pacer_late_lead(late) + FRAME_PACER_LATE_LEAD_STEP;
		if (late->lead > 0.5 * period)
			late->lead = 0.5 * period;
		late->calm = 0;
	}
	else if (++late->calm >= FRAME_PACER_LATE_CALM)
	{
		late->calm = 0;
		late->lead = frame_pacer_late_lead(late) - FRAME_PACER_LATE_LEAD_STEP / 4.0;
		if (late->lead < FRAME_PACER_LATE_LEAD_MINIMUM)
			late->lead = FRAME_PACER_LATE_LEAD_MINIMUM;
	}
}

/* ---------- a late start, extended (display.late_start = "extended")

The late start above engages only when a frame's whole cost fits in three
quarters of one refresh, which leaves out most of what an iPhone, iPad or
Apple TV does: dynamic resolution keeps the GPU at 85 to 95% of a refresh by
design, a 120 Hz refresh is 8.3 ms, and caps and scene classes give a frame
two refreshes. It also stops for 120 frames whenever one frame costs more
than the worst of the last 32, which a cost with any noise in it does about
one frame in 33, and it aims at the next refresh even when frames already
handed to the display own it, so a late start that begins while two frames
are in flight is always counted as shown late. These rules replace it:

- A frame aims at its own slot: the one after the last slot already spoken
  for (the last frame shown plus one slot for each frame in flight, or the
  frame before's aim), and no earlier than it can make. Starting from frames
  in flight, the first late start therefore waits for them to be shown and
  nothing is shown late.
- A slot is the frame's share of the display: one refresh, or under a cap or
  a scene class's rate below the display link's, that many (two for 30 on a
  60 Hz link), in the phase the frames before it were shown in.
- The budget may be longer than a slot: the CPU's work on a frame overlaps
  the GPU's on the one before. What must fit is each stage: the GPU's time on
  a frame within one slot (over that the device is below its rate and no
  start can help), and the whole budget within the frames in flight's limit
  of slots, so that no frame waits for a slot.
- The CPU's time on a frame (its start to its Present) fits one slot too, and
  a frame that cannot make the slot after the last one spoken for never
  sleeps: it starts at once with no aim. Otherwise one frame whose CPU work
  passes a slot would make every frame after it wait out a slot it could
  have had.
- A frame's cost is its own: from its start to the end of its own GPU work,
  less its waits for the display, with its own GPU time beside it.
- A cost over what was allowed stops late starts only when it used up more
  than FRAME_PACER_EXTENDED_JUMP of the lead. Less than that the lead
  absorbed, and the window has the new cost for the next frame.
- A frame's cost for the next start is the worst in the window or the worst
  among the last frames that had a late start, whichever is more. Frames that
  start at once measure less, because their waits for the display overlap the
  GPU's work on them: measured on a phone, 4.2 ms against 5.7 ms. Frames that
  do not fit push those remembered costs out one at a time, so one long frame
  cannot keep late starts off.
- A frame shown late, with its GPU work done a lead before its aim, is the
  display's own unless there are many: a 120 Hz phone shows about five frames
  in 1,000 late with no late start at all. FRAME_PACER_EXTENDED_LEAD_MISSES
  of them within FRAME_PACER_EXTENDED_LEAD_WINDOW aimed frames mean the
  system needs longer than the lead: late starts stop, the lead grows, that
  lead is remembered as too short, and the slow shrink stops
  FRAME_PACER_EXTENDED_LEAD_MARGIN over it. The shrink never goes under the
  lead it started from either: under it, a lead a little too short shows as
  a few more late frames, which cannot be told from the display's own.
- A frame shown late because it cost more than allowed stops late starts when
  another such frame was within FRAME_PACER_EXTENDED_AGAIN seconds before it.
  It says nothing about the lead.
- A stop lasts FRAME_PACER_EXTENDED_HOLDOFF seconds, doubling (to
  FRAME_PACER_EXTENDED_HOLDOFF_MAXIMUM) each time the next stop comes within
  FRAME_PACER_EXTENDED_RETRY seconds of starting again. The frames pushed
  late behind a late one (FRAME_PACER_EXTENDED_PUSHED slots) are the same
  event. A frame shown a quarter second or more after its aim is a load, not
  a frame. The lead also shrinks while frames do not fit, so a lead grown too
  long cannot keep late starts off.
- When the picture gets more pixels (dynamic resolution steps up) the costs
  in the window grow by the same ratio on their GPU part, so the step is
  allowed for before its first frame instead of being a jump. */

enum frame_pacer_start_state
{
	FRAME_PACER_START_OFF,     /* not asked for, or frames do not wait for the display */
	FRAME_PACER_START_ENGAGED, /* the frame has an aim */
	FRAME_PACER_START_NO_FIT,  /* too little history, the GPU over a slot, or the budget over the limit's slots */
	FRAME_PACER_START_HELD    /* after a jump or a frame shown late */
};

#define FRAME_PACER_EXTENDED_HOLDOFF 1.0
/* the shortest slot display.late_start = "extended" starts late for: a frame's share of the display at 60 frames a
second (a little under, for a display link that reports 59.94). Measured on a 120 Hz phone: at 30 frames a second
98 ms from a frame's start to the screen became 33 ms for 0.8 more late frames in 1,000, at 60 48 ms became 20 ms for
8 more, and at 120 frames with an aim were shown late three times as often as frames without, so the stops left it
on for a tenth of the frames and nothing was gained */
#define FRAME_PACER_EXTENDED_SLOT (0.98 / 60.0)
/* the lead it starts from (a phone's display took 4 to 5 ms from the GPU's end to the refresh) */
#define FRAME_PACER_EXTENDED_LEAD 0.005
/* a frame shown late for its cost stops late starts only when another was within this long before it */
#define FRAME_PACER_EXTENDED_AGAIN 1.0
/* frames shown late with their GPU work done a lead ahead mean a lead too short only when there are this many among
this many aimed frames (16 in 1,000): fewer are the display's own late frames */
#define FRAME_PACER_EXTENDED_LEAD_WINDOW 500
#define FRAME_PACER_EXTENDED_LEAD_MISSES 8
/* the frames behind a late one are pushed late with it for this many slots: they are the same event */
#define FRAME_PACER_EXTENDED_PUSHED 3.5
/* the slow shrink stops this far over a lead known to be too short */
#define FRAME_PACER_EXTENDED_LEAD_MARGIN 0.0015
#define FRAME_PACER_EXTENDED_HOLDOFF_MAXIMUM 8.0
#define FRAME_PACER_EXTENDED_RETRY 2.0
#define FRAME_PACER_EXTENDED_JUMP 0.5
/* allowed for a frame's shown notice to arrive after its refresh */
#define FRAME_PACER_EXTENDED_NOTICE 0.0015
/* the most the lead may grow to, in refreshes of the panel */
#define FRAME_PACER_EXTENDED_LEAD_MAXIMUM 1.5
/* frames shown on time before a lead that failed is forgotten */
#define FRAME_PACER_EXTENDED_FORGET 36000

struct frame_pacer_extended
{
	double work[FRAME_PACER_LATE_WINDOW]; /* start to the GPU's end, less waits for the display */
	double gpu[FRAME_PACER_LATE_WINDOW];  /* the GPU's time on the same frame */
	double cpu[FRAME_PACER_LATE_WINDOW];  /* its start to its Present */
	int count, next;
	double lead;            /* 0: FRAME_PACER_EXTENDED_LEAD */
	double lead_failed;     /* a lead that was too short; 0 none known */
	double holdoff_until;   /* late starts stop until this time */
	double holdoff;         /* how long the last miss stopped them */
	double missed_until;    /* when the last miss's stop ended, or will */
	double miss_at;         /* when the last frame was shown late (not one pushed late behind another); 0 none */
	double cost_miss_at;    /* when the last frame was shown late because it cost more than allowed; 0 none */
	int lead_misses;        /* frames shown late with their GPU work done a lead ahead, among the last aimed_since */
	unsigned long aimed_since; /* aimed frames shown since that count began */
	double aimed[FRAME_PACER_LATE_WINDOW]; /* the work of the last frames that had a late start (frames that start at
	                        once measure less: their waits for the display overlap the GPU's work on them) */
	int aimed_next;
	double last_aim;        /* the aim of the frame started last; 0 none */
	unsigned long calm;     /* frames shown on time since the lead last changed */
	unsigned long idle;     /* frames that did not fit since the lead last changed */
	int missed;             /* late starts are stopped by a frame shown late (not by a jump) */
	double slack_least;     /* the least time between the GPU's end and the aim among frames shown on time; 0 none */
	unsigned long remembered; /* frames shown on time since lead_failed was set */
	unsigned long late_starts, no_fit, behind, held, jumps, misses, pushed, early;
	double offset;          /* shown minus aim, summed over offsets frames */
	unsigned long offsets;
};

/* what a frame's start is decided from. now: the clock; refresh: the time of
any one refresh on that clock; period: the display link's; panel: the panel's
own period where it is shorter than the link's (0: the link's); share: the
frame's share in link periods (frame_pacer_share); limit: the frames in
flight's limit (0 none); in_flight: frames handed to the display and not yet
shown; shown_last: when the last frame was shown (0 unknown); slot_least:
frames whose share of the display is shorter than this many seconds start at
once (0: none are; FRAME_PACER_EXTENDED_SLOT for display.late_start =
"extended") */
struct frame_pacer_extended_input
{
	double now, refresh, period, panel, share;
	int limit, in_flight;
	double shown_last;
	double slot_least;
};

struct frame_pacer_extended_start
{
	double wait;    /* seconds to wait before the frame's work; 0 none */
	double aim;     /* when the frame aims to be shown; 0 none */
	double allowed; /* the cost allowed for; 0 none */
	enum frame_pacer_start_state state;
};

static inline double frame_pacer_extended_lead(const struct frame_pacer_extended *late)
{
	return late->lead > 0.0 ? late->lead : FRAME_PACER_EXTENDED_LEAD;
}

static inline double frame_pacer_extended_worst(const double *costs, int count)
{
	double worst = 0.0;
	int index;

	for (index = 0; index < count; index++)
		if (costs[index] > worst)
			worst = costs[index];
	return worst;
}

static inline double frame_pacer_extended_least(const double *costs, int count)
{
	double least = count ? costs[0] : 0.0;
	int index;

	for (index = 1; index < count; index++)
		if (costs[index] < least)
			least = costs[index];
	return least;
}

/* the refresh grid's step: the panel's period where it is the shorter */
static inline double frame_pacer_extended_step(double period, double panel)
{
	double ratio;

	if (panel < 1.0 / 240.0 || panel >= period)
		return period;
	/* (only when the link's refreshes are refreshes of the panel: an 80 Hz link on a 120 Hz panel is one and a half
	of the panel's, and its own grid is the only one both share) */
	ratio = period / panel;
	return ratio - (double)(long)(ratio + 0.5) < 0.02 && (double)(long)(ratio + 0.5) - ratio < 0.02 ? panel : period;
}

/* the slow shrink of the lead, a quarter step at a time, never under the lead it started from or
FRAME_PACER_EXTENDED_LEAD_MARGIN over a lead known to be too short */
static inline void frame_pacer_extended_shrink(struct frame_pacer_extended *late)
{
	double lead = frame_pacer_extended_lead(late);
	double least = late->lead_failed > 0.0 ? late->lead_failed + FRAME_PACER_EXTENDED_LEAD_MARGIN :
		FRAME_PACER_EXTENDED_LEAD;

	if (least < FRAME_PACER_EXTENDED_LEAD)
		least = FRAME_PACER_EXTENDED_LEAD;
	if (lead - FRAME_PACER_LATE_LEAD_STEP / 4.0 > least)
		late->lead = lead - FRAME_PACER_LATE_LEAD_STEP / 4.0;
	else if (lead > least)
		late->lead = least;
}

static inline struct frame_pacer_extended_start frame_pacer_extended_wait(struct frame_pacer_extended *late,
	const struct frame_pacer_extended_input *in)
{
	struct frame_pacer_extended_start start = { 0.0, 0.0, 0.0, FRAME_PACER_START_NO_FIT };
	double lead, work, gpu, budget, slot, step, earliest, occupied = 0.0, aim;
	int limit = in->limit > 0 ? in->limit : 2;
	double before = late->last_aim;

	late->last_aim = 0.0;
	if (in->now < late->holdoff_until)
	{
		late->held++;
		start.state = FRAME_PACER_START_HELD;
		return start;
	}
	if (late->count < FRAME_PACER_LATE_HISTORY || in->period < 1.0 / 240.0 || in->period > 1.0 / 20.0)
	{
		late->no_fit++;
		return start;
	}
	step = frame_pacer_extended_step(in->period, in->panel);
	slot = (in->share >= 1.0 ? in->share : 1.0) * in->period;
	/* (a rate it is not used at: the frame starts at once, as with no late start) */
	if (slot < in->slot_least)
	{
		late->no_fit++;
		return start;
	}
	lead = frame_pacer_extended_lead(late);
	work = frame_pacer_extended_worst(late->work, late->count);
	if (work < frame_pacer_extended_worst(late->aimed, FRAME_PACER_LATE_WINDOW))
		work = frame_pacer_extended_worst(late->aimed, FRAME_PACER_LATE_WINDOW);
	gpu = frame_pacer_extended_worst(late->gpu, late->count);
	budget = work + lead;
	late->missed = 0;
	/* each stage in its slot; and the frame reaches its Present, at the earliest its least CPU time after its start,
	no more than the limit's slots before its own slot, less the time the shown notice of the frame whose slot it
	takes needs to arrive: then no frame waits for a slot */
	if (gpu > slot || frame_pacer_extended_worst(late->cpu, late->count) > slot ||
		budget - frame_pacer_extended_least(late->cpu, late->count) + FRAME_PACER_EXTENDED_NOTICE > (double)limit * slot)
	{
		late->no_fit++;
		late->aimed[late->aimed_next] = 0.0;
		late->aimed_next = (late->aimed_next + 1) % FRAME_PACER_LATE_WINDOW;
		/* (a lead grown too long to fit comes back down by itself: frames that start at once teach it nothing) */
		if (++late->idle >= FRAME_PACER_LATE_CALM)
		{
			late->idle = 0;
			frame_pacer_extended_shrink(late);
		}
		return start;
	}
	earliest = in->now + budget;
	/* the last slot spoken for */
	if (in->shown_last > 0.0 && in->now - in->shown_last < 0.25)
	{
		/* (no more frames are in flight than the limit lets be) */
		int in_flight = in->in_flight < 0 ? 0 : in->limit > 0 && in->in_flight > in->limit ? in->limit : in->in_flight;

		occupied = in->shown_last + (double)in_flight * slot;
	}
	if (in->in_flight > 0 && before > occupied)
		occupied = before;
	if (occupied > 0.0)
	{
		/* behind: the frame cannot make the slot after the last one spoken for (by more than the lead's slack). It
		never sleeps then (the display has a free slot it is already late for): it starts at once with no aim */
		if (earliest > occupied + slot + 0.25 * lead)
		{
			late->behind++;
			return start;
		}
		/* the frames before keep the phase: one slot after the last one, on the nearest refresh */
		aim = occupied + slot;
		aim = in->refresh + step * (double)(long)((aim - in->refresh) / step + (aim >= in->refresh ? 0.5 : -0.5));
	}
	else
		/* nothing shown lately: the first refresh the frame can make */
		aim = in->refresh + step * (double)(long)((earliest - in->refresh) / step + 1.0);
	/* (frames in flight that are never shown must not hold the game: a wait past the limit's slots and one is a
	wrong guess) */
	if (aim - budget - in->now > ((double)limit + 1.0) * slot)
	{
		late->no_fit++;
		return start;
	}
	start.state = FRAME_PACER_START_ENGAGED;
	start.aim = late->last_aim = aim;
	start.allowed = work;
	start.wait = aim - budget > in->now ? aim - budget - in->now : 0.0;
	late->late_starts++;
	return start;
}

/* a frame's GPU work ended: work, from its start to that end less its waits for the display; gpu, the GPU's time on
it; cpu, from its start to its Present; allowed, what its start allowed for (0: it started at once); now: the clock */
static inline void frame_pacer_extended_cost(struct frame_pacer_extended *late, double work, double gpu, double cpu,
	double allowed, double now)
{
	/* (a quarter second or more is a load or the background, not a frame) */
	if (work <= 0.0 || work >= 0.25)
		return;
	late->work[late->next] = work;
	late->gpu[late->next] = gpu > 0.0 && gpu < work ? gpu : 0.0;
	late->cpu[late->next] = cpu > 0.0 && cpu < work ? cpu : 0.0;
	late->next = (late->next + 1) % FRAME_PACER_LATE_WINDOW;
	if (late->count < FRAME_PACER_LATE_WINDOW)
		late->count++;
	if (allowed > 0.0)
	{
		late->aimed[late->aimed_next] = work;
		late->aimed_next = (late->aimed_next + 1) % FRAME_PACER_LATE_WINDOW;
	}
	if (allowed > 0.0 && work > allowed + FRAME_PACER_EXTENDED_JUMP * frame_pacer_extended_lead(late))
	{
		late->jumps++;
		if (late->holdoff_until < now + FRAME_PACER_EXTENDED_HOLDOFF)
			late->holdoff_until = now + FRAME_PACER_EXTENDED_HOLDOFF;
	}
}

/* the picture has ratio times the pixels from the next frame on */
static inline void frame_pacer_extended_scale(struct frame_pacer_extended *late, double ratio)
{
	int index;

	if (!(ratio > 1.0) || ratio > 16.0)
		return;
	for (index = 0; index < FRAME_PACER_LATE_WINDOW; index++)
		if (late->aimed[index] > 0.0)
			late->aimed[index] += frame_pacer_extended_worst(late->gpu, late->count) * (ratio - 1.0);
	for (index = 0; index < late->count; index++)
	{
		late->work[index] += late->gpu[index] * (ratio - 1.0);
		late->gpu[index] *= ratio;
	}
}

/* a frame that aimed for a refresh was shown at this time. slack: how long before its aim its GPU work ended
(negative: after it; NAN unknown); step: frame_pacer_extended_step; slot: the frame's share of the display, in seconds;
now: the clock */
static inline void frame_pacer_extended_shown(struct frame_pacer_extended *late, double aim, double at, double slack,
	double step, double slot, double now)
{
	double lead = frame_pacer_extended_lead(late);

	/* (a quarter second or more after its aim is a load or the background, not a frame) */
	if (aim <= 0.0 || at <= 0.0 || at > aim + 0.25)
		return;
	late->offset += at - aim;
	late->offsets++;
	if (++late->aimed_since > FRAME_PACER_EXTENDED_LEAD_WINDOW)
	{
		late->aimed_since = 0;
		late->lead_misses = 0;
	}
	if (at > aim + 0.5 * step)
	{
		int stop = 0;

		/* (pushed late behind a late frame: the same event, whether or not it stopped late starts) */
		if ((now < late->holdoff_until && late->missed) ||
			(late->miss_at > 0.0 && now < late->miss_at + FRAME_PACER_EXTENDED_PUSHED * (slot > step ? slot : step)))
		{
			late->pushed++;
			return;
		}
		late->misses++;
		late->calm = late->idle = 0;
		late->miss_at = now;
		/* (a frame whose GPU end is not known says nothing about the lead: it counts as one that cost too much) */
		if (slack == slack && slack >= lead - 0.0005)
		{
			/* done a lead ahead and still late: the display's own, unless there are many */
			if (++late->lead_misses >= FRAME_PACER_EXTENDED_LEAD_MISSES)
			{
				late->lead_misses = 0;
				late->aimed_since = 0;
				late->lead_failed = lead;
				late->remembered = 0;
				late->lead = lead + FRAME_PACER_LATE_LEAD_STEP;
				if (late->lead > FRAME_PACER_EXTENDED_LEAD_MAXIMUM * step)
					late->lead = FRAME_PACER_EXTENDED_LEAD_MAXIMUM * step;
				stop = 1;
			}
		}
		else
		{
			/* it cost more than was allowed */
			stop = late->cost_miss_at > 0.0 && now < late->cost_miss_at + FRAME_PACER_EXTENDED_AGAIN;
			late->cost_miss_at = now;
		}
		if (!stop)
			return;
		late->holdoff = late->holdoff > 0.0 && now < late->missed_until + FRAME_PACER_EXTENDED_RETRY ?
			late->holdoff * 2.0 : FRAME_PACER_EXTENDED_HOLDOFF;
		if (late->holdoff > FRAME_PACER_EXTENDED_HOLDOFF_MAXIMUM)
			late->holdoff = FRAME_PACER_EXTENDED_HOLDOFF_MAXIMUM;
		late->holdoff_until = late->missed_until = now + late->holdoff;
		late->missed = 1;
		return;
	}
	if (at < aim - 0.5 * step)
		late->early++;
	if (late->lead_failed > 0.0 && ++late->remembered >= FRAME_PACER_EXTENDED_FORGET)
		late->lead_failed = 0.0;
	if (slack == slack && slack > 0.0 && (late->slack_least <= 0.0 || slack < late->slack_least))
		late->slack_least = slack;
	if (++late->calm >= FRAME_PACER_LATE_CALM)
	{
		late->calm = 0;
		frame_pacer_extended_shrink(late);
	}
}


/* ---- the extended late start's bookkeeping of one frame, the same for the host (host_frame_pacer.c) and the
simulator (tests/frame_pacer_sim.h), so what the simulator tests is what the host does: what the rules are asked,
what a frame's work is, and how long before its aim the GPU finished it */
struct frame_pacer_extended_frame
{
	double began;   /* when its work began, after its late start's wait */
	double cpu;     /* seconds from there to its Present */
	double aim;     /* the refresh its late start aimed at; 0 none */
	double allowed; /* the cost its late start allowed for; 0 none */
	double gpu_end; /* when the GPU finished it; 0: not yet */
};

/* what frame_pacer_extended_wait is asked. handed, shown_highest: the frames handed to the display and the highest
of them a shown notice has come for (those between are in flight); the rest as struct frame_pacer_extended_input */
static inline struct frame_pacer_extended_input frame_pacer_extended_asked(double now, double refresh, double period,
	double panel, double share, int limit, unsigned long handed, unsigned long shown_highest, double shown_last,
	double slot_least)
{
	struct frame_pacer_extended_input in = { now, refresh, period, panel, share, limit, (int)(handed - shown_highest),
		shown_last, slot_least };

	return in;
}

/* the GPU finished a frame at gpu_end, after gpu seconds on it; waited: the seconds of its work spent waiting for the
display (0 or less: none); now: the clock */
static inline void frame_pacer_extended_frame_finished(struct frame_pacer_extended *late,
	struct frame_pacer_extended_frame *frame, double gpu_end, double gpu, double waited, double now)
{
	frame->gpu_end = gpu_end;
	frame_pacer_extended_cost(late, gpu_end - frame->began - (waited > 0.0 ? waited : 0.0), gpu, frame->cpu,
		frame->allowed, now);
}

/* a frame was shown at this time. frame: its bookkeeping, or NULL when it is gone (the GPU's end is then unknown, as
it is when its report has not come); aim: the refresh it aimed at (0 none); period, panel, share: as
struct frame_pacer_extended_input; now: the clock */
static inline void frame_pacer_extended_frame_shown(struct frame_pacer_extended *late,
	const struct frame_pacer_extended_frame *frame, double aim, double at, double period, double panel, double share,
	double now)
{
	double slack = frame && frame->gpu_end > 0.0 ? aim - frame->gpu_end : (double)NAN;

	frame_pacer_extended_shown(late, aim, at, slack, frame_pacer_extended_step(period, panel), share * period, now);
}

/* ---------- when frames were shown */

struct frame_pacer_shown
{
	double last;         /* when the frame before was shown; 0: none yet */
	unsigned long frames, intervals, late;
	double longest;      /* seconds */
};

/* a frame was shown at this time (seconds; 0 or less: it never was, and
nothing is counted). Returns the time since the frame before it, or 0 for the
first. An interval over one and a half times the frame's share of the
display (refreshes) is late: a frame was repeated or dropped. A quarter of a
second or more is a load or the background, not a frame, and starts again. */
static inline double frame_pacer_shown_add(struct frame_pacer_shown *shown, double at, double period, double refreshes)
{
	double interval = 0.0;

	if (at <= 0.0)
		return 0.0;
	shown->frames++;
	if (shown->last > 0.0 && at > shown->last)
	{
		interval = at - shown->last;
		if (interval < 0.25)
		{
			shown->intervals++;
			if (period > 0.0 && interval > 1.5 * refreshes * period)
				shown->late++;
			if (interval > shown->longest)
				shown->longest = interval;
		}
	}
	shown->last = at;
	return interval;
}
