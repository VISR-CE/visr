#include "host_topology.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct host_topology_facts duo_closed(double now) {
    struct host_topology_facts f;
    memset(&f, 0, sizeof(f));
    f.now = now;
    f.idiom = HOST_FACTS_IDIOM_PHONE;
    f.metal = 1;
    f.app_scene.present = 1;
    f.app_scene.screen = 0xa1;
    f.app_scene.main_screen = 0xb2;   /* the main screen is the other panel */
    f.app_scene.main_is_app = 0;
    host_topology_facts_add_screen(&f, 0xa1, 0, 1206, 2622, 120);
    f.hinge_available = 1;
    f.hinge_status = HOST_HINGE_CLOSED;
    f.posture = HOST_POSTURE_FLAT;
    host_topology_facts_add_output(&f, HOST_ROUTE_SPEAKER);
    return f;
}

int main(void) {
    struct host_topology_observer observer = {0};
    struct host_topology_facts f = duo_closed(1.0);
    char text[512];
    /* the first read is written, then nothing while only the clock moves: a line per change, not per poll */
    assert(host_topology_observe(&observer, &f));
    for (int i = 0; i < 100; i++) {
        struct host_topology_facts same = duo_closed(2.0 + i);
        assert(!host_topology_observe(&observer, &same));
    }
    /* every kind of field change is written once */
    struct host_topology_facts g = duo_closed(200.0);
    g.hinge_status = HOST_HINGE_FULLY_OPEN;
    assert(host_topology_observe(&observer, &g));
    assert(!host_topology_observe(&observer, &g));
    g = duo_closed(201.0);
    g.screens[0].pixel_width = 2007;
    assert(host_topology_observe(&observer, &g));
    g = duo_closed(202.0);
    g.posture = HOST_POSTURE_LAPTOP_L;
    assert(host_topology_observe(&observer, &g));
    g = duo_closed(203.0);
    host_topology_facts_add_screen(&g, 0xb2, 1, 1920, 1080, 60);
    assert(host_topology_observe(&observer, &g));
    g.accessory.registered = 1;
    g.accessory.offered_since = 204.0;
    assert(host_topology_observe(&observer, &g));
    g.now = 210.0;
    assert(!host_topology_observe(&observer, &g));
    g.display_port_name[0] = 'T';
    assert(host_topology_observe(&observer, &g));
    /* a fifth screen and a fifth output are counted, never written past the arrays, and still a change */
    g = duo_closed(300.0);
    for (int i = 0; i < 6; i++) host_topology_facts_add_screen(&g, 0x10 + i, 0, 100, 100, 60);
    assert(g.screen_count == HOST_FACTS_SCREENS && g.screens_dropped == 3);
    for (int i = 0; i < 6; i++) host_topology_facts_add_output(&g, HOST_ROUTE_HDMI);
    assert(g.output_count == 7);
    host_topology_facts_format(&g, text, sizeof(text));
    assert(strstr(text, "3 not listed") && strstr(text, "3 more"));
    /* the line says what the facts are */
    f = duo_closed(5.0);
    host_topology_facts_format(&f, text, sizeof(text));
    assert(strstr(text, "idiom phone") && strstr(text, "hinge closed") && strstr(text, "screens 1 [a1 1206x2622 at 120 Hz]"));
    assert(strstr(text, "main screen b2") && !strstr(text, "(the app's)"));
    f.app_scene.main_screen = 0xa1; f.app_scene.main_is_app = 1;
    host_topology_facts_format(&f, text, sizeof(text));
    assert(strstr(text, "main screen a1 (the app's)"));
    f = duo_closed(5.0);
    host_topology_facts_format(&f, text, sizeof(text));
    assert(strstr(text, "route speaker") && strstr(text, "external scene none") && !strchr(text, '\n'));
    /* a short buffer is cut and terminated, never overrun */
    char small[16];
    host_topology_facts_format(&f, small, sizeof(small));
    assert(strlen(small) == sizeof(small) - 1);
    /* port types */
    assert(host_route_port_kind("HDMIOutput") == HOST_ROUTE_HDMI && host_route_port_kind("AirPlay") == HOST_ROUTE_AIRPLAY);
    assert(host_route_port_kind("BluetoothA2DPOutput") == HOST_ROUTE_BLUETOOTH && host_route_port_kind(NULL) == HOST_ROUTE_OTHER);
    assert(host_route_port_kind("Speaker") == HOST_ROUTE_SPEAKER && host_route_port_kind("DisplayPort") == HOST_ROUTE_DISPLAYPORT);
    puts("PASS: display facts are written once per change, with a bounded line and bounded lists");
    return 0;
}
