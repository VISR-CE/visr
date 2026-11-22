#include "host_topology_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* fact sequences for the arrangements the design lists, each asserting every display's kind and the game and device */
#define APP 0xa1
#define EXT 0xb2
#define MAIN_OTHER 0xc3

static struct host_topology_facts base(int idiom, double now) {
    struct host_topology_facts f;
    memset(&f, 0, sizeof(f));
    f.now = now;
    f.idiom = idiom;
    f.metal = 1;
    host_topology_facts_add_output(&f, HOST_ROUTE_SPEAKER);
    return f;
}
static struct host_topology_facts duo(int hinge_status, double now) {
    struct host_topology_facts f = base(HOST_FACTS_IDIOM_PHONE, now);
    f.app_scene.present = 1; f.app_scene.screen = APP; f.app_scene.main_screen = APP; f.app_scene.main_is_app = 1;
    f.hinge_available = 1; f.hinge_status = hinge_status;
    host_topology_facts_add_screen(&f, APP, 0, hinge_status == HOST_HINGE_CLOSED ? 1398 : 2007,
        hinge_status == HOST_HINGE_CLOSED ? 2034 : 2853, 120);
    return f;
}
static void add_monitor(struct host_topology_facts *f, int negotiating) {
    host_topology_facts_add_screen(f, EXT, 0, 2560, 1600, 60);
    f->external_scene.present = 1; f->external_scene.negotiating = negotiating; f->external_scene.screen = EXT;
    f->external_scene.scene = 0xd4; f->external_scene.width = 2560; f->external_scene.height = 1600; f->external_scene.hertz = 60;
    host_topology_facts_add_output(f, HOST_ROUTE_HDMI);
    snprintf(f->display_port_name, sizeof(f->display_port_name), "AOC28E850");
}
static struct host_topology derive(const struct host_topology_facts *f, const struct host_topology *previous) {
    struct host_topology t;
    host_topology_derive(f, previous, &t);
    return t;
}

int main(void) {
    struct host_topology t, u;
    /* the Duo closed and open: the app's scene screen is the panel, named by the hinge; the main screen being the
       other panel changes nothing */
    struct host_topology_facts f = duo(HOST_HINGE_CLOSED, 1.0);
    t = derive(&f, NULL);
    assert(t.display_count == 1 && t.displays[0].kind == DISPLAY_PANEL_COVER && t.game == 0 && t.device == 0);
    assert(!t.external_context && !t.mirrored && t.settled && t.generation == 1 && t.sound == SOUND_DEVICE);
    f = duo(HOST_HINGE_FULLY_OPEN, 2.0);
    f.app_scene.main_screen = MAIN_OTHER; f.app_scene.main_is_app = 0;
    u = derive(&f, &t);
    assert(u.displays[0].kind == DISPLAY_PANEL_INNER && !u.external_context && u.generation == 2);
    u = derive(&f, &u);
    assert(u.generation == 2);   /* nothing changed: the generation holds */
    /* a Duo that lists both panels: the other one is unclaimed, never the game's or the device's, and holds
       settled at 0 */
    f = duo(HOST_HINGE_FULLY_OPEN, 3.0);
    host_topology_facts_add_screen(&f, MAIN_OTHER, 0, 1398, 2034, 120);
    t = derive(&f, NULL);
    assert(t.display_count == 2 && t.displays[0].kind == DISPLAY_PANEL_INNER && t.displays[1].kind == DISPLAY_UNCLAIMED);
    assert(t.game == 0 && t.device == 0 && !t.settled && !t.external_context && !t.mirrored);
    /* a TV listed before its scene connects stays unclaimed, never a panel: no external context, unsettled */
    f = duo(HOST_HINGE_FULLY_OPEN, 4.0);
    host_topology_facts_add_screen(&f, EXT, 0, 3840, 2160, 60);
    t = derive(&f, NULL);
    assert(t.displays[1].kind == DISPLAY_UNCLAIMED && !t.settled && t.game == 0 && !t.external_context);
    /* a wired monitor: negotiating, then settled with the game on it */
    f = duo(HOST_HINGE_FULLY_OPEN, 5.0);
    add_monitor(&f, 1);
    t = derive(&f, NULL);
    assert(t.displays[1].kind == DISPLAY_EXTERNAL && t.game == 0 && !t.settled);
    f = duo(HOST_HINGE_FULLY_OPEN, 6.0);
    add_monitor(&f, 0);
    u = derive(&f, &t);
    assert(t.generation == 1 && u.generation == 2);
    assert(u.game == 1 && u.device == 0 && u.external_context && u.settled && !u.mirrored);
    assert(u.transport == TRANSPORT_WIRED && !strcmp(u.name, "AOC28E850") && u.sound == SOUND_DISPLAY);
    assert(u.output.width == 2560 && u.output.height == 1600 && u.output.hertz == 60);
    /* folding the Duo with the game on the monitor: the device's panel changes, the game does not */
    f = duo(HOST_HINGE_CLOSED, 7.0);
    add_monitor(&f, 0);
    t = derive(&f, &u);
    assert(t.game == 1 && t.displays[t.device].kind == DISPLAY_PANEL_COVER && t.external_context && t.generation == 3);
    /* the L posture on the device puts the picture on the posture layer; with the monitor it does not */
    f = duo(HOST_HINGE_PARTIALLY_OPEN, 8.0);
    f.posture = HOST_POSTURE_LAPTOP_L;
    t = derive(&f, NULL);
    assert(t.picture_layer && t.posture == HOST_POSTURE_LAPTOP_L);
    add_monitor(&f, 0);
    t = derive(&f, &t);
    assert(!t.picture_layer && t.external_context);
    /* AirPlay: the only display-carrying port is AirPlay; a monitor with no speakers is unknown */
    f = duo(HOST_HINGE_FULLY_OPEN, 9.0);
    add_monitor(&f, 0);
    f.output_count = 0;
    host_topology_facts_add_output(&f, HOST_ROUTE_AIRPLAY);
    t = derive(&f, NULL);
    assert(t.transport == TRANSPORT_AIRPLAY && t.sound == SOUND_AIRPLAY);
    f.output_count = 0;
    host_topology_facts_add_output(&f, HOST_ROUTE_BLUETOOTH);
    t = derive(&f, NULL);
    assert(t.transport == TRANSPORT_UNKNOWN && !strcmp(t.name, "External display") && t.sound == SOUND_HEADPHONES);
    /* an AirPlay speaker with no display: the route never creates a display */
    f = duo(HOST_HINGE_FULLY_OPEN, 10.0);
    f.output_count = 0;
    host_topology_facts_add_output(&f, HOST_ROUTE_AIRPLAY);
    t = derive(&f, NULL);
    assert(t.display_count == 1 && !t.external_context && t.transport == TRANSPORT_NONE && !t.mirrored && t.sound == SOUND_AIRPLAY);
    /* a mirror with a listed screen, and a mirror with none (HDMI in the route, no second screen) */
    f = duo(HOST_HINGE_FULLY_OPEN, 11.0);
    host_topology_facts_add_screen(&f, EXT, 1, 1920, 1080, 60);
    t = derive(&f, NULL);
    assert(t.mirrored && !t.external_context && t.settled && t.game == t.device);
    f = duo(HOST_HINGE_FULLY_OPEN, 12.0);
    f.output_count = 0;
    host_topology_facts_add_output(&f, HOST_ROUTE_HDMI);
    t = derive(&f, NULL);
    assert(t.mirrored && t.display_count == 1);
    /* the accessory offered for 3 s with no scene is a mirror; offered for less it is a scene on its way */
    f = duo(HOST_HINGE_FULLY_OPEN, 20.0);
    f.accessory.registered = 1; f.accessory.available = 1; f.accessory.offered_since = 18.0;
    t = derive(&f, NULL);
    assert(!t.mirrored && !t.settled);
    f.now = 21.5;
    t = derive(&f, NULL);
    assert(t.mirrored && t.settled);
    /* an iPad: its panel, an extended desktop it is not on, then the window dragged onto it */
    f = base(HOST_FACTS_IDIOM_PAD, 30.0);
    f.app_scene.present = 1; f.app_scene.screen = APP; f.app_scene.main_screen = APP; f.app_scene.main_is_app = 1;
    host_topology_facts_add_screen(&f, APP, 0, 2048, 2732, 120);
    host_topology_facts_add_screen(&f, EXT, 0, 2560, 1440, 60);
    t = derive(&f, NULL);
    assert(t.displays[0].kind == DISPLAY_PANEL && t.displays[1].kind == DISPLAY_EXTENDED && t.game == 0 && !t.external_context);
    f.app_scene.screen = EXT; f.app_scene.main_is_app = 0;
    t = derive(&f, &t);
    assert(t.displays[t.game].kind == DISPLAY_EXTENDED && t.external_context && t.game == 1);
    /* an iPad app on a Mac: the Mac's monitors are never this app's displays */
    f = base(HOST_FACTS_IDIOM_PAD, 40.0);
    f.ios_app_on_mac = 1;
    f.app_scene.present = 1; f.app_scene.screen = APP; f.app_scene.main_screen = APP; f.app_scene.main_is_app = 1;
    host_topology_facts_add_screen(&f, APP, 0, 3024, 1964, 120);
    host_topology_facts_add_screen(&f, EXT, 0, 2560, 1440, 60);
    t = derive(&f, NULL);
    assert(t.display_count == 1 && t.displays[0].kind == DISPLAY_PANEL && !t.external_context && !t.mirrored && t.settled);

    /* reports: no two arrangements that differ in game display, device panel, transport or mirror state share a
       block, and every arrangement change moves the generation */
    char block[8][160];
    struct host_topology_facts a[8];
    a[0] = duo(HOST_HINGE_CLOSED, 1.0);
    a[1] = duo(HOST_HINGE_FULLY_OPEN, 1.0);
    a[2] = duo(HOST_HINGE_FULLY_OPEN, 1.0); add_monitor(&a[2], 0);
    a[3] = duo(HOST_HINGE_CLOSED, 1.0); add_monitor(&a[3], 0);
    a[4] = duo(HOST_HINGE_FULLY_OPEN, 1.0); add_monitor(&a[4], 0); a[4].output_count = 0; host_topology_facts_add_output(&a[4], HOST_ROUTE_AIRPLAY);
    a[5] = duo(HOST_HINGE_FULLY_OPEN, 1.0); host_topology_facts_add_screen(&a[5], EXT, 1, 1920, 1080, 60);
    a[6] = duo(HOST_HINGE_FULLY_OPEN, 1.0); add_monitor(&a[6], 0); a[6].output_count = 0; host_topology_facts_add_output(&a[6], HOST_ROUTE_BLUETOOTH);
    a[7] = duo(HOST_HINGE_FULLY_OPEN, 1.0); a[7].output_count = 0; host_topology_facts_add_output(&a[7], HOST_ROUTE_HEADPHONES);
    struct host_topology gen[8];
    for (int i = 0; i < 8; i++) {
        gen[i] = derive(&a[i], NULL);
        host_topology_json(&gen[i], block[i], sizeof(block[i]));
    }
    for (int i = 0; i < 8; i++)
        for (int j = i + 1; j < 8; j++) {
            if (i == 1 && j == 7) continue;   /* only the sound differs between these two: headphones are not the arrangement */
            assert(strcmp(block[i], block[j]));
            struct host_topology next = derive(&a[j], &gen[i]);
            assert(next.generation == gen[i].generation + 1);
        }
    /* parity with today's classifier: over phone and pad, the main screen being the app's or the other, the other
       screen mirroring or not, and the external scene on it or not, the model's counts of screens that are not the
       device's own panels equal the legacy count through host_external_screen_is_external */
    for (int idiom = HOST_FACTS_IDIOM_PHONE; idiom <= HOST_FACTS_IDIOM_PAD; idiom++)
        for (int main_is_app = 0; main_is_app < 2; main_is_app++)
            for (int mirrors = 0; mirrors < 2; mirrors++)
                for (int scene = 0; scene < 2; scene++) {
                    f = base(idiom, 50.0);
                    f.app_scene.present = 1; f.app_scene.screen = APP;
                    f.app_scene.main_screen = main_is_app ? APP : EXT;
                    f.app_scene.main_is_app = main_is_app;
                    host_topology_facts_add_screen(&f, APP, 0, 2048, 2732, 120);
                    host_topology_facts_add_screen(&f, EXT, mirrors, 2560, 1440, 60);
                    if (scene) {
                        f.external_scene.present = 1; f.external_scene.screen = EXT; f.external_scene.width = 2560;
                        f.external_scene.height = 1440; f.external_scene.hertz = 60;
                    }
                    t = derive(&f, NULL);
                    int other = 0, mirroring = 0;
                    const unsigned ids[2] = {APP, EXT};
                    const int mirror_of[2] = {0, mirrors};
                    for (int i = 0; i < 2; i++) {
                        struct host_external_screen_signals sig = {.main_screen = ids[i] == f.app_scene.main_screen,
                            .external_scene = scene && ids[i] == EXT, .mirroring = mirror_of[i],
                            .phone = idiom == HOST_FACTS_IDIOM_PHONE, .mac = 0};
                        if (host_external_screen_is_external(&sig)) { other++; if (mirror_of[i]) mirroring++; }
                    }
                    if (t.other_screens != other || t.mirroring_screens != mirroring)
                        fprintf(stderr, "idiom %d main_is_app %d mirrors %d scene %d: model %d/%d legacy %d/%d\n", idiom, main_is_app, mirrors, scene, t.other_screens, t.mirroring_screens, other, mirroring);
                    assert(t.other_screens == other && t.mirroring_screens == mirroring);
                }
    /* the lines */
    char line[400];
    f = duo(HOST_HINGE_FULLY_OPEN, 1.0); add_monitor(&f, 0);
    t = derive(&f, NULL);
    host_topology_format(&t, line, sizeof(line));
    assert(strstr(line, "topology 1: game on external \"AOC28E850\" 2560x1600 at 60 Hz (wired); device panel_inner 2007x2853 at 120 Hz"));
    assert(strstr(line, "sound to the display; profile external"));
    char small[12];
    host_topology_format(&t, small, sizeof(small));
    assert(strlen(small) == sizeof(small) - 1);

    /* shadow mode: a disagreement counts after it has held, once, and again only when the answers change */
    struct host_topology_shadow shadow;
    memset(&shadow, 0, sizeof(shadow));
    assert(!host_topology_shadow_check(&shadow, 0, 1.0, 1, 1));
    assert(!host_topology_shadow_check(&shadow, 0, 2.0, 0, 1));    /* starts */
    assert(!host_topology_shadow_check(&shadow, 0, 2.5, 0, 1));    /* not long enough */
    assert(host_topology_shadow_check(&shadow, 0, 3.1, 0, 1));     /* held: logged */
    assert(!host_topology_shadow_check(&shadow, 0, 9.0, 0, 1));    /* once */
    assert(!host_topology_shadow_check(&shadow, 0, 10.0, 1, 1));   /* agreed again */
    assert(!host_topology_shadow_check(&shadow, 0, 11.0, 0, 1));
    assert(host_topology_shadow_check(&shadow, 0, 12.2, 0, 1));    /* a new episode logs again */
    assert(!host_topology_shadow_check(&shadow, 1, 12.2, 0, 1));   /* questions are separate */
    assert(!host_topology_shadow_check(&shadow, 99, 12.2, 0, 1));
    puts("PASS: the topology model over the Duo, monitor, AirPlay, mirror, iPad and Mac fact sequences");
    return 0;
}
