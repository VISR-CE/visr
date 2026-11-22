#include "host_panel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    struct host_hinge_snapshot hinge = {0};
    /* no hinge, whatever a stale status says: one panel */
    assert(host_panel_from_hinge(&hinge) == HOST_PANEL_BUILTIN);
    hinge.status = HOST_HINGE_CLOSED;
    assert(host_panel_from_hinge(&hinge) == HOST_PANEL_BUILTIN);
    assert(host_panel_from_hinge(NULL) == HOST_PANEL_BUILTIN);
    /* a hinge: closed is the cover, every other state the inner display */
    hinge.available = 1;
    assert(host_panel_from_hinge(&hinge) == HOST_PANEL_COVER);
    hinge.status = HOST_HINGE_PARTIALLY_OPEN;
    assert(host_panel_from_hinge(&hinge) == HOST_PANEL_INNER);
    hinge.status = HOST_HINGE_FULLY_OPEN;
    assert(host_panel_from_hinge(&hinge) == HOST_PANEL_INNER);
    hinge.status = HOST_HINGE_UNKNOWN;
    assert(host_panel_from_hinge(&hinge) == HOST_PANEL_INNER);
    /* the names a report carries are distinct */
    assert(!strcmp(host_panel_name(HOST_PANEL_BUILTIN), "panel"));
    assert(!strcmp(host_panel_name(HOST_PANEL_COVER), "panel_cover"));
    assert(!strcmp(host_panel_name(HOST_PANEL_INNER), "panel_inner"));
    puts("PASS: panel from the hinge status");
    return 0;
}
