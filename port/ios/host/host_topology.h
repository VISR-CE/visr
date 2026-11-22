/* What the system reports about displays, copied into plain data (phase 1a of the display topology design).
   Nothing is derived from it and no consumer reads it: debug.topology_facts writes a line when a field other than
   the poll's clock changes, so a run's display facts can be read back, compared and replayed in a probe.
   Plain C in the style of host_external_session.h and host_posture.h, built alone by a probe. */
#pragma once
#include "host_hinge.h"
#include "host_posture.h"
#include <stdio.h>
#include <string.h>

enum host_facts_idiom { HOST_FACTS_IDIOM_OTHER, HOST_FACTS_IDIOM_PHONE, HOST_FACTS_IDIOM_PAD };

/* what the audio route's output ports are, by their AVAudioSession port type */
enum host_route_port {
    HOST_ROUTE_OTHER, HOST_ROUTE_SPEAKER, HOST_ROUTE_HEADPHONES, HOST_ROUTE_BLUETOOTH, HOST_ROUTE_HDMI,
    HOST_ROUTE_DISPLAYPORT, HOST_ROUTE_AIRPLAY, HOST_ROUTE_USB, HOST_ROUTE_CAR
};

#define HOST_FACTS_SCREENS 4
#define HOST_FACTS_OUTPUTS 4

struct host_topology_facts {
    double now;                /* the poll's clock; the timers below are start times on it */
    int idiom;                 /* enum host_facts_idiom */
    int ios_app_on_mac, metal;
    struct { int present; unsigned screen, main_screen; int main_is_app; } app_scene;  /* main_screen: UIScreen.mainScreen */
    struct { int present, negotiating; unsigned screen, scene; int width, height, hertz; } external_scene;
    struct { int registered, available; double offered_since; } accessory;
    struct { unsigned id; int mirrors, pixel_width, pixel_height, hertz; } screens[HOST_FACTS_SCREENS];
    int screen_count;          /* screens beyond HOST_FACTS_SCREENS are counted in screens_dropped, not listed */
    int screens_dropped;
    int hinge_available, hinge_status;       /* status only; the angle stays inside host_posture */
    int posture;               /* enum host_posture, host_posture_update's debounced answer */
    int outputs[HOST_FACTS_OUTPUTS], output_count;   /* enum host_route_port */
    char display_port_name[64];              /* the route's HDMI, DisplayPort or AirPlay port name, if one */
};

static inline int host_route_port_kind(const char *port_type) {
    if (!port_type) return HOST_ROUTE_OTHER;
    if (!strcmp(port_type, "Speaker") || !strcmp(port_type, "Receiver")) return HOST_ROUTE_SPEAKER;
    if (!strcmp(port_type, "Headphones")) return HOST_ROUTE_HEADPHONES;
    if (!strncmp(port_type, "Bluetooth", 9)) return HOST_ROUTE_BLUETOOTH;
    if (!strcmp(port_type, "HDMIOutput")) return HOST_ROUTE_HDMI;
    if (!strcmp(port_type, "DisplayPort")) return HOST_ROUTE_DISPLAYPORT;
    if (!strcmp(port_type, "AirPlay")) return HOST_ROUTE_AIRPLAY;
    if (!strcmp(port_type, "USBAudio")) return HOST_ROUTE_USB;
    if (!strcmp(port_type, "CarAudio")) return HOST_ROUTE_CAR;
    return HOST_ROUTE_OTHER;
}

static inline const char *host_route_port_name(int kind) {
    switch (kind) {
    case HOST_ROUTE_SPEAKER: return "speaker";
    case HOST_ROUTE_HEADPHONES: return "headphones";
    case HOST_ROUTE_BLUETOOTH: return "bluetooth";
    case HOST_ROUTE_HDMI: return "hdmi";
    case HOST_ROUTE_DISPLAYPORT: return "displayport";
    case HOST_ROUTE_AIRPLAY: return "airplay";
    case HOST_ROUTE_USB: return "usb";
    case HOST_ROUTE_CAR: return "car";
    default: return "other";
    }
}

/* add one output to the facts; a fifth is counted but not kept */
static inline void host_topology_facts_add_output(struct host_topology_facts *facts, int kind) {
    if (facts->output_count < HOST_FACTS_OUTPUTS) facts->outputs[facts->output_count] = kind;
    facts->output_count++;
}

/* add one screen to the facts; one beyond the fourth is counted in screens_dropped */
static inline void host_topology_facts_add_screen(struct host_topology_facts *facts, unsigned id, int mirrors,
    int pixel_width, int pixel_height, int hertz) {
    if (facts->screen_count < HOST_FACTS_SCREENS) {
        facts->screens[facts->screen_count].id = id;
        facts->screens[facts->screen_count].mirrors = mirrors;
        facts->screens[facts->screen_count].pixel_width = pixel_width;
        facts->screens[facts->screen_count].pixel_height = pixel_height;
        facts->screens[facts->screen_count].hertz = hertz;
        facts->screen_count++;
    } else facts->screens_dropped++;
}

/* every field but the poll's clock, compared as bytes: the facts are built from a zeroed struct, and the assert
   keeps a new field from introducing padding that a struct copy need not carry */
_Static_assert(sizeof(struct host_topology_facts) == 264, "host_topology_facts changed size: check for padding and update this");
static inline int host_topology_facts_changed(const struct host_topology_facts *a, const struct host_topology_facts *b) {
    struct host_topology_facts x = *a, y = *b;
    x.now = y.now = 0.0;
    return memcmp(&x, &y, sizeof(x)) != 0;
}

static inline const char *host_facts_idiom_name(int idiom) {
    return idiom == HOST_FACTS_IDIOM_PHONE ? "phone" : idiom == HOST_FACTS_IDIOM_PAD ? "pad" : "other";
}
static inline const char *host_facts_hinge_name(int available, int status) {
    if (!available) return "none";
    switch (status) {
    case HOST_HINGE_CLOSED: return "closed";
    case HOST_HINGE_PARTIALLY_OPEN: return "partially_open";
    case HOST_HINGE_FULLY_OPEN: return "fully_open";
    default: return "unknown";
    }
}
static inline const char *host_facts_posture_name(int posture) {
    switch (posture) {
    case HOST_POSTURE_LAPTOP_L: return "laptop_l";
    case HOST_POSTURE_BOOK_L: return "book_l";
    default: return "flat";
    }
}

/* one line, no newline; text is always terminated */
static inline void host_topology_facts_format(const struct host_topology_facts *f, char *text, size_t size) {
    size_t n = 0;
    if (!size) return;
    text[0] = 0;
#define ADD(...) do { if (n < size) { int w = snprintf(text + n, size - n, __VA_ARGS__); if (w > 0) n += (size_t)w; } } while (0)
    ADD("topology facts: idiom %s%s, metal %d; app scene %s", host_facts_idiom_name(f->idiom),
        f->ios_app_on_mac ? " (app on Mac)" : "", f->metal, f->app_scene.present ? "on" : "none");
    if (f->app_scene.present) ADD(" screen %x", f->app_scene.screen);
    ADD(", main screen %x%s", f->app_scene.main_screen, f->app_scene.main_is_app ? " (the app's)" : "");
    ADD("; external scene ");
    if (f->external_scene.present)
        ADD("%x on screen %x %dx%d at %d Hz%s", f->external_scene.scene, f->external_scene.screen,
            f->external_scene.width, f->external_scene.height, f->external_scene.hertz,
            f->external_scene.negotiating ? ", negotiating" : "");
    else ADD("none");
    ADD("; accessory %s", !f->accessory.registered ? "not registered" : f->accessory.available ? "available" : "unavailable");
    if (f->accessory.offered_since > 0.0) ADD(" (offered since %.1f)", f->accessory.offered_since);
    ADD("; screens %d", f->screen_count + f->screens_dropped);
    for (int i = 0; i < f->screen_count; i++)
        ADD(" [%x %dx%d at %d Hz%s]", f->screens[i].id, f->screens[i].pixel_width, f->screens[i].pixel_height,
            f->screens[i].hertz, f->screens[i].mirrors ? ", mirrors" : "");
    if (f->screens_dropped) ADD(" (%d not listed)", f->screens_dropped);
    ADD("; hinge %s, posture %s; route", host_facts_hinge_name(f->hinge_available, f->hinge_status),
        host_facts_posture_name(f->posture));
    if (!f->output_count) ADD(" none");
    for (int i = 0; i < f->output_count && i < HOST_FACTS_OUTPUTS; i++) ADD(" %s", host_route_port_name(f->outputs[i]));
    if (f->output_count > HOST_FACTS_OUTPUTS) ADD(" (%d more)", f->output_count - HOST_FACTS_OUTPUTS);
    if (f->display_port_name[0]) ADD(" \"%s\"", f->display_port_name);
#undef ADD
    if (n >= size) text[size - 1] = 0;
}

/* the observer's state: the facts last written. Returns 1 when a line is due (the first read, and every change). */
struct host_topology_observer { int have; struct host_topology_facts last; };
static inline int host_topology_observe(struct host_topology_observer *o, const struct host_topology_facts *facts) {
    if (o->have && !host_topology_facts_changed(&o->last, facts)) return 0;
    o->last = *facts;
    o->have = 1;
    return 1;
}
