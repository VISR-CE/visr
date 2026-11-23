/* The display topology, derived from facts (phase 1b of the display topology design, in shadow: it is logged and
   compared with today's deciders, and no consumer reads it yet). One function classifies a display; it is the only
   place that does, and it is deterministic over the facts and the previous topology, so a probe replays recorded
   fact sequences. Plain C in the style of host_external_session.h. */
#pragma once
#include "host_topology.h"
#include "host_external_session.h"

enum host_display_kind {
    DISPLAY_PANEL, DISPLAY_PANEL_COVER, DISPLAY_PANEL_INNER,
    DISPLAY_EXTERNAL,
    DISPLAY_EXTENDED,      /* an iPad's Stage Manager desktop */
    DISPLAY_UNCLAIMED      /* a screen the system lists that no scene of ours is on, and no panel */
};
enum host_display_transport { TRANSPORT_NONE, TRANSPORT_WIRED, TRANSPORT_AIRPLAY, TRANSPORT_UNKNOWN };
enum host_sound_target { SOUND_DEVICE, SOUND_HEADPHONES, SOUND_DISPLAY, SOUND_AIRPLAY, SOUND_CAR, SOUND_OTHER };

struct host_topology {
    unsigned generation;
    struct { enum host_display_kind kind; int pixel_width, pixel_height, hertz; unsigned screen; } displays[HOST_FACTS_SCREENS + 1];
    int display_count;
    int game, device;                  /* indexes into displays; -1 when unknown (before the app's scene exists) */
    enum host_display_transport transport;   /* of displays[game] */
    char name[64];                     /* of displays[game] when external */
    struct { int width, height, hertz; } output;   /* the game's drawable, as far as the facts say */
    int posture;                       /* enum host_posture */
    int picture_layer;                 /* 1 when the device-side picture is on the posture layer, else SDL's */
    enum host_sound_target sound;
    int external_context;              /* the profile's [external] section applies */
    int mirrored;                      /* a display shows this device scaled; the game is not on it */
    int settled;                       /* 0 while a mode is being negotiated or a screen is unclaimed */
    int other_screens, mirroring_screens;   /* the legacy counts (host_external_displays), for the shadow comparison */
};

static inline const char *host_display_kind_name(int kind) {
    switch (kind) {
    case DISPLAY_PANEL_COVER: return "panel_cover";
    case DISPLAY_PANEL_INNER: return "panel_inner";
    case DISPLAY_EXTERNAL: return "external";
    case DISPLAY_EXTENDED: return "extended";
    case DISPLAY_UNCLAIMED: return "unclaimed";
    default: return "panel";
    }
}
static inline const char *host_display_transport_name(int transport) {
    switch (transport) {
    case TRANSPORT_WIRED: return "wired";
    case TRANSPORT_AIRPLAY: return "airplay";
    case TRANSPORT_UNKNOWN: return "unknown";
    default: return "none";
    }
}
static inline const char *host_sound_target_name(int target) {
    switch (target) {
    case SOUND_HEADPHONES: return "headphones";
    case SOUND_DISPLAY: return "display";
    case SOUND_AIRPLAY: return "airplay";
    case SOUND_CAR: return "car";
    case SOUND_OTHER: return "other";
    default: return "device";
    }
}

/* the device's own display: a hinge says which panel is lit, else there is one panel */
static inline enum host_display_kind host_topology_device_kind(const struct host_topology_facts *f) {
    if (!f->hinge_available) return DISPLAY_PANEL;
    return f->hinge_status == HOST_HINGE_CLOSED ? DISPLAY_PANEL_COVER : DISPLAY_PANEL_INNER;
}

static inline int host_topology_has_output(const struct host_topology_facts *f, int kind) {
    for (int i = 0; i < f->output_count && i < HOST_FACTS_OUTPUTS; i++) if (f->outputs[i] == kind) return 1;
    return 0;
}

_Static_assert(sizeof(struct host_topology) == 228, "host_topology changed size: check for padding and update this");
/* everything but the generation, compared as bytes (the struct is zeroed before it is filled) */
static inline int host_topology_changed(const struct host_topology *a, const struct host_topology *b) {
    struct host_topology x = *a, y = *b;
    x.generation = y.generation = 0;
    return memcmp(&x, &y, sizeof(x)) != 0;
}

/* Derives the topology. previous may be NULL (the first one). The generation moves when anything but itself changes. */
static inline void host_topology_derive(const struct host_topology_facts *f, const struct host_topology *previous,
    struct host_topology *out) {
    struct host_topology t;
    memset(&t, 0, sizeof(t));
    t.game = t.device = -1;
    t.posture = f->posture;
    int unclaimed = 0, other_screens = 0, mirroring_screens = 0;
    const int pad = f->idiom == HOST_FACTS_IDIOM_PAD;
    int external = -1, app_listed = 0;
    for (int i = 0; i < f->screen_count && t.display_count < HOST_FACTS_SCREENS; i++) {
        enum host_display_kind kind;
        const unsigned id = f->screens[i].id;
        const int mirrors = f->screens[i].mirrors;
        if (f->app_scene.present && id == f->app_scene.screen) {
            /* the screen of the app's own scene. On a phone that is always one of its panels. On an iPad it is the
               iPad's panel while it is the main screen, and the extended desktop when the window was moved off it. */
            app_listed = 1;
            kind = (pad && !f->app_scene.main_is_app) ? DISPLAY_EXTENDED : host_topology_device_kind(f);
            if (f->ios_app_on_mac) kind = DISPLAY_PANEL;
            t.device = t.display_count;
        } else if (f->ios_app_on_mac) {
            continue;                          /* a Mac's monitors hold windows; they are not this app's displays */
        } else if (f->external_scene.present && id == f->external_scene.screen) {
            kind = DISPLAY_EXTERNAL;
            external = t.display_count;
        } else if (pad && !mirrors && id == f->app_scene.main_screen) {
            kind = DISPLAY_PANEL;              /* the iPad's own panel while the window sits on its extended desktop */
        } else if (pad && !mirrors) {
            kind = DISPLAY_EXTENDED;
        } else {
            kind = DISPLAY_UNCLAIMED;
            if (!mirrors) unclaimed = 1;
        }
        /* the legacy count of screens that are not this device's own panels: a scene's display, an iPad's extended
           desktop, and a screen the system mirrors this device to */
        if (kind == DISPLAY_EXTERNAL || kind == DISPLAY_EXTENDED || (kind == DISPLAY_UNCLAIMED && mirrors)) {
            other_screens++;
            if (mirrors) mirroring_screens++;
        }
        t.displays[t.display_count].kind = kind;
        t.displays[t.display_count].screen = id;
        t.displays[t.display_count].pixel_width = f->screens[i].pixel_width;
        t.displays[t.display_count].pixel_height = f->screens[i].pixel_height;
        t.displays[t.display_count].hertz = f->screens[i].hertz;
        t.display_count++;
    }
    if (f->app_scene.present && !app_listed && !f->ios_app_on_mac && t.display_count <= HOST_FACTS_SCREENS) {
        t.displays[t.display_count].kind = host_topology_device_kind(f);
        t.displays[t.display_count].screen = f->app_scene.screen;
        t.device = t.display_count++;
    }
    /* The external scene present with its mode chosen is where the game draws; a scene still negotiating is not yet. */
    if (external >= 0 && !f->external_scene.negotiating) t.game = external;
    else t.game = t.device;
    t.settled = !f->external_scene.negotiating && !unclaimed;
    const int game_external = t.game >= 0 && t.game == external;
    if (game_external) {
        /* the scene's own mode and rate are the drawable's shape */
        t.output.width = t.displays[t.game].pixel_width;
        t.output.height = t.displays[t.game].pixel_height;
        t.output.hertz = t.displays[t.game].hertz;
        if (f->external_scene.width > 0 && f->external_scene.height > 0) {
            t.output.width = f->external_scene.width;
            t.output.height = f->external_scene.height;
        }
        if (f->external_scene.hertz > 0) t.output.hertz = f->external_scene.hertz;
    } else if (t.game >= 0) {
        t.output.width = t.displays[t.game].pixel_width;
        t.output.height = t.displays[t.game].pixel_height;
        t.output.hertz = t.displays[t.game].hertz;
    }
    const int wired = host_topology_has_output(f, HOST_ROUTE_HDMI) || host_topology_has_output(f, HOST_ROUTE_DISPLAYPORT);
    if (game_external) {
        t.transport = wired ? TRANSPORT_WIRED : host_topology_has_output(f, HOST_ROUTE_AIRPLAY) ? TRANSPORT_AIRPLAY : TRANSPORT_UNKNOWN;
        snprintf(t.name, sizeof(t.name), "%s", t.transport != TRANSPORT_UNKNOWN && f->display_port_name[0] ?
            f->display_port_name : "External display");
    }
    t.other_screens = other_screens;
    t.mirroring_screens = mirroring_screens;
    t.external_context = t.game >= 0 && (t.displays[t.game].kind == DISPLAY_EXTERNAL || t.displays[t.game].kind == DISPLAY_EXTENDED);
    /* mirrored: today's three signs, through the same function (host_external_session.h) */
    struct host_external_display_signals signals = {.game_external = t.external_context,
        .other_screens = other_screens, .mirroring_screens = mirroring_screens,
        .hdmi_route = host_topology_has_output(f, HOST_ROUTE_HDMI), .ios_app_on_mac = f->ios_app_on_mac};
    if (f->accessory.available && !t.external_context && !f->external_scene.present && f->accessory.offered_since > 0.0)
        signals.accessory_offered_seconds = f->now - f->accessory.offered_since;
    t.mirrored = host_external_display_use(&signals) == HOST_EXTERNAL_DISPLAY_MIRRORED;
    if (!t.mirrored && f->accessory.available && !f->external_scene.present && f->accessory.offered_since > 0.0 &&
        f->now - f->accessory.offered_since < HOST_EXTERNAL_OFFER_SETTLE) t.settled = 0;
    t.picture_layer = (f->posture == HOST_POSTURE_LAPTOP_L || f->posture == HOST_POSTURE_BOOK_L) && !game_external && t.game >= 0;
    if (host_topology_has_output(f, HOST_ROUTE_HDMI) || host_topology_has_output(f, HOST_ROUTE_DISPLAYPORT)) t.sound = SOUND_DISPLAY;
    else if (host_topology_has_output(f, HOST_ROUTE_AIRPLAY)) t.sound = SOUND_AIRPLAY;
    else if (host_topology_has_output(f, HOST_ROUTE_CAR)) t.sound = SOUND_CAR;
    else if (host_topology_has_output(f, HOST_ROUTE_HEADPHONES) || host_topology_has_output(f, HOST_ROUTE_BLUETOOTH) ||
        host_topology_has_output(f, HOST_ROUTE_USB)) t.sound = SOUND_HEADPHONES;
    else if (f->output_count && !host_topology_has_output(f, HOST_ROUTE_SPEAKER)) t.sound = SOUND_OTHER;
    t.generation = previous ? previous->generation : 0;
    if (!previous || host_topology_changed(previous, &t)) t.generation++;
    *out = t;
}

static inline const char *host_posture_label(int posture) { return host_facts_posture_name(posture); }

/* one line per topology in force, no newline */
static inline void host_topology_format(const struct host_topology *t, char *text, size_t size) {
    size_t n = 0;
    if (!size) return;
    text[0] = 0;
#define ADD(...) do { if (n < size) { int w = snprintf(text + n, size - n, __VA_ARGS__); if (w > 0) n += (size_t)w; } } while (0)
    ADD("topology %u: ", t->generation);
    if (t->game < 0) ADD("no display yet");
    else {
        ADD("game on %s", host_display_kind_name(t->displays[t->game].kind));
        if (t->name[0]) ADD(" \"%s\"", t->name);
        ADD(" %dx%d at %d Hz", t->output.width, t->output.height, t->output.hertz);
        if (t->transport != TRANSPORT_NONE) ADD(" (%s)", host_display_transport_name(t->transport));
    }
    if (t->device >= 0 && t->device != t->game)
        ADD("; device %s %dx%d at %d Hz", host_display_kind_name(t->displays[t->device].kind),
            t->displays[t->device].pixel_width, t->displays[t->device].pixel_height, t->displays[t->device].hertz);
    ADD(", %s%s", host_posture_label(t->posture), t->picture_layer ? " (picture on the posture layer)" : "");
    ADD("; sound to the %s; profile %s", host_sound_target_name(t->sound), t->external_context ? "external" : "built-in");
    if (t->mirrored) ADD("; mirrored");
    if (!t->settled) ADD("; not settled");
#undef ADD
    if (n >= size) text[size - 1] = 0;
}

/* the report's topology block: a JSON object, no newline */
static inline void host_topology_json(const struct host_topology *t, char *text, size_t size) {
    snprintf(text, size, "{\"game\":\"%s\",\"transport\":\"%s\",\"device\":\"%s\",\"posture\":\"%s\",\"sound\":\"%s\",\"mirrored\":%s}",
        t->game >= 0 ? host_display_kind_name(t->displays[t->game].kind) : "none", host_display_transport_name(t->transport),
        t->device >= 0 ? host_display_kind_name(t->displays[t->device].kind) : "none", host_posture_label(t->posture),
        host_sound_target_name(t->sound), t->mirrored ? "true" : "false");
}

/* Shadow mode: today's decider and the model answer the same question; a disagreement that lasts is logged once.
   A commit takes a frame or two to follow the model, so a disagreement counts only after it has held for
   HOST_SHADOW_SETTLE seconds, and is logged again only when the pair of answers changes. */
#define HOST_SHADOW_SETTLE 1.0
#define HOST_SHADOW_QUESTIONS 8
struct host_shadow_question { int disagreeing, legacy, model, logged; double since; };
struct host_topology_shadow { struct host_shadow_question q[HOST_SHADOW_QUESTIONS]; };
static inline int host_topology_shadow_check(struct host_topology_shadow *s, int question, double now, int legacy, int model) {
    if (question < 0 || question >= HOST_SHADOW_QUESTIONS) return 0;
    struct host_shadow_question *q = &s->q[question];
    if (legacy == model) { q->disagreeing = 0; q->logged = 0; return 0; }
    if (!q->disagreeing || q->legacy != legacy || q->model != model) {
        q->disagreeing = 1; q->legacy = legacy; q->model = model; q->since = now; q->logged = 0;
        return 0;
    }
    if (q->logged || now - q->since < HOST_SHADOW_SETTLE) return 0;
    q->logged = 1;
    return 1;
}
