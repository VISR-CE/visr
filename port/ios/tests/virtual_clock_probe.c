#include "halo_virtual_clock.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    unsigned long frame;

    /* thirty frames are one second on every clock */
    assert(halo_virtual_clock_counter(30) - halo_virtual_clock_counter(0) == HALO_VIRTUAL_CLOCK_RATE);
    assert(halo_virtual_clock_milliseconds(30) - halo_virtual_clock_milliseconds(0) == 1000UL);
    assert(halo_virtual_clock_seconds(30) - halo_virtual_clock_seconds(0) == 1ULL);
    for (frame = 0; frame < 3000; frame++) {
        unsigned long step = halo_virtual_clock_milliseconds(frame + 1) - halo_virtual_clock_milliseconds(frame);

        /* the performance counter moves exactly one count, 1/30 s, per frame */
        assert(halo_virtual_clock_counter(frame + 1) - halo_virtual_clock_counter(frame) == 1ULL);
        /* milliseconds round, but the remainders don't accumulate */
        assert(step == 33UL || step == 34UL);
    }
    assert(halo_virtual_clock_milliseconds(3000) - halo_virtual_clock_milliseconds(0) == 100000UL);
    /* like a real clock, it doesn't start at zero */
    assert(halo_virtual_clock_milliseconds(0) != 0);
    assert(halo_virtual_clock_counter(0) != 0);
    assert(halo_virtual_clock_seconds(0) > 946684800ULL); /* after 2000-01-01 */
    /* ten hours of frames: the header's arithmetic is 64-bit by construction,
       and this checks the values (unsigned long is 64-bit here, 32 on the guest) */
    assert(halo_virtual_clock_milliseconds(1080000UL) - halo_virtual_clock_milliseconds(0) == 36000000UL);
    assert(halo_virtual_clock_seconds(1080000UL) - halo_virtual_clock_seconds(0) == 36000ULL);
    /* debug.exit_after in frames: 0 or less is never, and any positive time quits */
    assert(halo_virtual_clock_exit_frame(40.0) == 1200UL);
    assert(halo_virtual_clock_exit_frame(0.0) == 0UL);
    assert(halo_virtual_clock_exit_frame(-5.0) == 0UL);
    assert(halo_virtual_clock_exit_frame(0.01) == 1UL);
    /* debug.exit_after's own deadlines, pushed out by the script's wait on both clocks */
    {
        struct halo_exit_state fixed = { 1, 1199, 1200, 0, 0, 0, 0, 0, 0, 0 };
        struct halo_exit_state wall = { 0, 0, 0, 0, 59999, 60000, 0, 0, 0, 0 };

        assert(!halo_exit_reached(&fixed) && (fixed.frames = 1200, halo_exit_reached(&fixed)));
        fixed.frame_delay = 150;
        assert(!halo_exit_reached(&fixed) && (fixed.frames = 1350, halo_exit_reached(&fixed)));
        assert(!halo_exit_reached(&wall) && (wall.ticks = 60000, halo_exit_reached(&wall)));
        wall.tick_delay = 5000;
        assert(!halo_exit_reached(&wall) && (wall.ticks = 65000, halo_exit_reached(&wall)));
        wall.exit_ticks = 0;   /* no deadline: never */
        assert(!halo_exit_reached(&wall));
    }
    /* A scripted run only ever lasts longer: at least `span` frames (debug.exit_after at 30 a second) after the script began, and not
       before its clock's own deadline either. exit_after 137 s: span 4110, wall deadline 137 s. */
    {
        /* a configured script that has not begun keeps every deadline from ending the run (a campaign still loading); once it begins, the
       frame budget decides as before */
    {
        struct halo_exit_state loading = { 0, 1140, 0, 0, 131000, 120000, 0, 0, 3600, 1 };
        struct halo_exit_state loading_fixed = { 1, 5000, 3600, 0, 0, 0, 0, 0, 3600, 1 };

        assert(!halo_exit_reached(&loading) && !halo_exit_reached(&loading_fixed));
        loading.script_began_next = 1200 + 1;
        loading.script_pending = 0;
        assert(!halo_exit_reached(&loading));   /* began at 1200: needs frame 4800 */
        loading.frames = 4800;
        assert(halo_exit_reached(&loading));
    }
    /* a 7 frames a second host: the wall deadline (137 s plus the 100 s wait) passes at frame 1659; the frames decide */
        struct halo_exit_state slow = { 0, 1659, 0, 0, 237000, 137000, 100000, 1000 + 1, 4110, 0 };

        assert(!halo_exit_reached(&slow));
        slow.frames = 1000 + 4110 - 1;
        assert(!halo_exit_reached(&slow));
        slow.frames = 1000 + 4110;
        assert(halo_exit_reached(&slow));
        /* a 60 Hz host: the frame budget passed long before the wall deadline, which keeps the old length (exit_after seconds) */
        struct halo_exit_state fast60 = { 0, 5200, 0, 0, 100000, 137000, 10000, 100 + 1, 4110, 0 };

        assert(!halo_exit_reached(&fast60));
        fast60.ticks = 147000;
        assert(halo_exit_reached(&fast60));
        /* the same at 90 Hz, and a script that begins at frame 0 counts */
        struct halo_exit_state fast90 = { 0, 7000, 0, 0, 60000, 137000, 0, 0 + 1, 4110, 0 };

        assert(!halo_exit_reached(&fast90));
        fast90.ticks = 137000;
        assert(halo_exit_reached(&fast90));
        struct halo_exit_state zero = { 0, 4109, 0, 0, 999999, 1, 0, 0 + 1, 4110, 0 };

        assert(!halo_exit_reached(&zero));
        zero.frames = 4110;
        assert(halo_exit_reached(&zero));
        /* no script (script_began_next 0) or no span: the clock's own deadline decides */
        struct halo_exit_state none = { 0, 10, 0, 0, 999999, 1, 0, 0, 4110, 0 };

        assert(halo_exit_reached(&none));
        none.script_began_next = 1; none.span = 0;
        assert(halo_exit_reached(&none));
    }
    puts("PASS: virtual clock rate, remainders, starting values, long runs and exit frames");
    return 0;
}
