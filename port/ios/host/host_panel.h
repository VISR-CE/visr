/* Which of the device's own panels the app's scene is on, for the benchmark report (the display topology design,
   phase 0). Decided from the hinge alone: only the screen of the app's own scene is ever given a panel name, and
   neither model identifiers nor pixel sizes are compared. */
#pragma once
#include "host_hinge.h"

enum host_panel { HOST_PANEL_BUILTIN, HOST_PANEL_COVER, HOST_PANEL_INNER };

/* A device with a hinge shows the cover while it is closed and the inner display in any other state, a half-open
   or an unknown status included. A device without one has a single panel. */
static inline enum host_panel host_panel_from_hinge(const struct host_hinge_snapshot *hinge) {
    if (!hinge || !hinge->available) return HOST_PANEL_BUILTIN;
    return hinge->status == HOST_HINGE_CLOSED ? HOST_PANEL_COVER : HOST_PANEL_INNER;
}

static inline const char *host_panel_name(enum host_panel panel) {
    switch (panel) {
    case HOST_PANEL_COVER: return "panel_cover";
    case HOST_PANEL_INNER: return "panel_inner";
    default: return "panel";
    }
}
