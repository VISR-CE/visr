/* Scene ownership is independent of audio routing. A late disconnect from a
   replaced scene must not tear down the new receiver's output. */
#pragma once
#include <stdint.h>
struct host_external_session { uintptr_t owner; int negotiating, pending; };
static inline void host_external_session_connect(struct host_external_session *s,uintptr_t owner) {
    /* Recover locally while the replacement scene negotiates, even if the
       old disconnect has not reached a screen commit yet. */
    s->owner=owner;s->negotiating=1;s->pending=1;
}
static inline int host_external_session_disconnect(struct host_external_session *s,uintptr_t owner) {
    if(!owner || s->owner!=owner)return 0;
    s->owner=0;s->negotiating=0;s->pending=1;return 1;
}
static inline void host_external_session_accept(struct host_external_session *s) {
    s->negotiating=0;s->pending=1;
}
static inline void host_external_session_complete(struct host_external_session *s) { s->pending=0; }
/* The scene accessory the system offers a connected display through is evaluated when it is registered and when the
   view controller appears. A display plugged in while the app is already running (the accessory registered at launch)
   can be left mirrored without the registration ever becoming available. When a display has been seen (a second
   screen, or a video audio route) for a while with no scene and no availability, the registration is made again,
   at most twice per connection. */
struct host_external_refresh { double seen_at; int attempts; };
static inline int host_external_refresh_due(struct host_external_refresh *r,int display_seen,int available,
    int connected,double now) {
    if(!display_seen || available || connected) {r->seen_at=0;r->attempts=0;return 0;}
    if(r->seen_at==0) {r->seen_at=now;return 0;}
    if(now-r->seen_at<1.5 || r->attempts>=2)return 0;
    r->attempts++;r->seen_at=now;return 1;
}
/* How a connected display is being used, for the benchmark report. A display the system mirrors this device to
   while the game is not on it shows the device's picture scaled to that display: the game still draws the device's
   own size at the device's rate, so such a run says nothing about the external display and is not a built-in run
   either. A second screen that is neither the game's nor a mirror (an iPad's extended display under Stage Manager)
   is "other". Signs of a mirror, any one enough:
   - a second screen whose mirroredScreen is set;
   - an HDMI output in the audio route with no second screen listed: an iPhone has no extended display, and the
     system may list no screen for a display it only mirrors (AirPlay alone does not count, since an AirPlay speaker
     routes sound the same way);
   - the external scene accessory offered for HOST_EXTERNAL_OFFER_SETTLE seconds or more with no scene of VISR's
     connected and the game not on the display (AirPlay mirroring to a TV or an Apple TV): shorter offers are VISR's
     own scene on its way.
   An iPhone or iPad app running on a Mac ("Designed for iPad") has no external display and never reports a mirror. */
#define HOST_EXTERNAL_OFFER_SETTLE 3.0
enum host_external_display_use {
    HOST_EXTERNAL_DISPLAY_NONE, HOST_EXTERNAL_DISPLAY_IN_USE, HOST_EXTERNAL_DISPLAY_MIRRORED, HOST_EXTERNAL_DISPLAY_OTHER
};
struct host_external_display_signals {
    int game_external, other_screens, mirroring_screens, hdmi_route, ios_app_on_mac;
    double accessory_offered_seconds;   /* how long the accessory has been offered with the game not on it; 0 if not */
};
static inline enum host_external_display_use host_external_display_use(const struct host_external_display_signals *s) {
    if(s->game_external)return HOST_EXTERNAL_DISPLAY_IN_USE;
    if(s->ios_app_on_mac)return HOST_EXTERNAL_DISPLAY_NONE;
    if(s->mirroring_screens>0 || (s->hdmi_route && s->other_screens==0) ||
        s->accessory_offered_seconds>=HOST_EXTERNAL_OFFER_SETTLE)return HOST_EXTERNAL_DISPLAY_MIRRORED;
    return s->other_screens>0?HOST_EXTERNAL_DISPLAY_OTHER:HOST_EXTERNAL_DISPLAY_NONE;
}
/* (the time the accessory has been offered with the game off the display: since the first poll that saw it, reset
   whenever it is not offered or the game is on the display) */
static inline double host_external_offer_track(double *since,int offered,double now) {
    if(!offered){*since=0;return 0;}
    if(*since<=0)*since=now;
    return now-*since;
}
static inline const char *host_external_display_use_name(enum host_external_display_use use) {
    switch(use) {
    case HOST_EXTERNAL_DISPLAY_IN_USE: return "in_use";
    case HOST_EXTERNAL_DISPLAY_MIRRORED: return "mirrored";
    case HOST_EXTERNAL_DISPLAY_OTHER: return "other";
    default: return "none";
    }
}
