#include "host_external_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    struct host_external_session session={0};
    host_external_session_connect(&session,1);
    assert(session.owner==1 && session.negotiating && session.pending);
    host_external_session_accept(&session);
    assert(!session.negotiating && session.pending);
    /* A renderer refusal leaves the transaction pending. */
    assert(session.pending);
    host_external_session_complete(&session);
    assert(!session.pending);
    host_external_session_connect(&session,2);
    assert(!host_external_session_disconnect(&session,1));
    assert(session.owner==2 && session.negotiating);
    assert(host_external_session_disconnect(&session,2));
    assert(!session.owner && !session.negotiating && session.pending);
    /* Reconnection may arrive before the return-to-device commit. */
    host_external_session_connect(&session,3);
    assert(session.pending && session.negotiating && session.owner==3);
    assert(!host_external_session_disconnect(&session,2));
    assert(host_external_session_disconnect(&session,3));
    host_external_session_complete(&session);
    assert(!session.pending);
    puts("PASS: scene ownership, replacement, disconnect before output, and reconnect before commit");
    /* a display plugged in while the app runs, never offered a scene: the registration is made again, twice at most */
    struct host_external_refresh refresh={0,0};
    assert(!host_external_refresh_due(&refresh,0,0,0,10.0));
    assert(!host_external_refresh_due(&refresh,1,0,0,10.0));
    assert(!host_external_refresh_due(&refresh,1,0,0,11.0));
    assert(host_external_refresh_due(&refresh,1,0,0,11.6));
    assert(!host_external_refresh_due(&refresh,1,0,0,12.0));
    assert(host_external_refresh_due(&refresh,1,0,0,13.2));
    assert(!host_external_refresh_due(&refresh,1,0,0,20.0));
    /* it resets when the display goes, becomes available or connects */
    assert(!host_external_refresh_due(&refresh,0,0,0,21.0));
    assert(!host_external_refresh_due(&refresh,1,0,0,22.0));
    assert(host_external_refresh_due(&refresh,1,0,0,23.6));
    assert(!host_external_refresh_due(&refresh,1,1,0,30.0) && !refresh.seen_at && !refresh.attempts);
    assert(!host_external_refresh_due(&refresh,1,0,1,31.0) && !refresh.seen_at);
    puts("PASS: the scene accessory is registered again for a display that was never offered a scene");
    /* how a connected display is used: the game's own, a mirror of the device, or another screen */
#define USE(...) host_external_display_use(&(struct host_external_display_signals){__VA_ARGS__})
    assert(USE(0)==HOST_EXTERNAL_DISPLAY_NONE);
    assert(USE(.game_external=1,.other_screens=1,.hdmi_route=1)==HOST_EXTERNAL_DISPLAY_IN_USE);
    /* (the game on the display wins over a mirror seen in the same moment, as while the scene takes over) */
    assert(USE(.game_external=1,.other_screens=1,.mirroring_screens=1,.hdmi_route=1,.accessory_offered_seconds=9)==
        HOST_EXTERNAL_DISPLAY_IN_USE);
    assert(USE(.other_screens=1,.mirroring_screens=1)==HOST_EXTERNAL_DISPLAY_MIRRORED);
    /* an HDMI route with no screen listed is a cabled display mirroring the device */
    assert(USE(.hdmi_route=1)==HOST_EXTERNAL_DISPLAY_MIRRORED);
    /* a second screen that neither mirrors nor shows the game (Stage Manager's extended display) is not a mirror,
       even with HDMI sound */
    assert(USE(.other_screens=1)==HOST_EXTERNAL_DISPLAY_OTHER);
    assert(USE(.other_screens=1,.hdmi_route=1)==HOST_EXTERNAL_DISPLAY_OTHER);
    /* the accessory offered with the game off the display: a mirror once it has lasted the settle time (AirPlay to a
       TV), not while VISR's own scene connects */
    assert(USE(.accessory_offered_seconds=0.5)==HOST_EXTERNAL_DISPLAY_NONE);
    assert(USE(.accessory_offered_seconds=HOST_EXTERNAL_OFFER_SETTLE-0.01)==HOST_EXTERNAL_DISPLAY_NONE);
    assert(USE(.accessory_offered_seconds=HOST_EXTERNAL_OFFER_SETTLE)==HOST_EXTERNAL_DISPLAY_MIRRORED);
    assert(USE(.game_external=1,.accessory_offered_seconds=60)==HOST_EXTERNAL_DISPLAY_IN_USE);
    /* "Designed for iPad" on a Mac never reports a mirror, whatever the screens and routes say */
    assert(USE(.ios_app_on_mac=1,.other_screens=1,.mirroring_screens=1,.hdmi_route=1,.accessory_offered_seconds=60)==
        HOST_EXTERNAL_DISPLAY_NONE);
    assert(USE(.ios_app_on_mac=1,.other_screens=1)==HOST_EXTERNAL_DISPLAY_NONE);
    /* the offer clock: starts at the first offer, resets when the offer goes or the game moves to the display */
    double since=0;
    assert(host_external_offer_track(&since,0,10.0)==0 && since==0);
    assert(host_external_offer_track(&since,1,10.0)==0 && since==10.0);
    assert(host_external_offer_track(&since,1,12.5)==2.5);
    assert(USE(.accessory_offered_seconds=host_external_offer_track(&since,1,13.0))==HOST_EXTERNAL_DISPLAY_MIRRORED);
    assert(host_external_offer_track(&since,0,13.1)==0 && since==0);
    assert(host_external_offer_track(&since,1,14.0)==0);   /* a new offer starts the clock again */
#undef USE
    assert(!strcmp(host_external_display_use_name(HOST_EXTERNAL_DISPLAY_MIRRORED),"mirrored"));
    assert(!strcmp(host_external_display_use_name(HOST_EXTERNAL_DISPLAY_IN_USE),"in_use"));
    assert(!strcmp(host_external_display_use_name(HOST_EXTERNAL_DISPLAY_OTHER),"other"));
    assert(!strcmp(host_external_display_use_name(HOST_EXTERNAL_DISPLAY_NONE),"none"));
    puts("PASS: a mirrored display is told apart from the game's own display and from no display");
}
