/* Real presentation wrappers and signed store transitions, without a GPU or app cache. */
#import "../host/host_data_pack.h"
#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>
#include "../../third_party/monocypher/monocypher-ed25519.h"
#include "../host/host_gpu_dispatch.c"
#include <assert.h>

@interface HostDataPackStore (MenuContract)
- (NSDictionary *)menuSnapshot;
- (void)selectMenuFolder:(NSString *)folder;
- (void)retainMenuFolder:(NSString *)folder;
+ (void)forgetSharedStores;
@end
static HostDataPackStore *launchStore;
static int healthSignals;
void host_logf(int priority, const char *format, ...) { (void)priority; (void)format; }
void platform_log(const char *format, ...) { (void)format; }
const struct gpu_backend gpu_backend_metal = {0}, gpu_backend_gl = {0};
void host_lifecycle_hold(void) {}
void gpu_metal_frame_deadline_end(void) {}
void gpu_metal_frame_deadline_leave(void) {}
void host_config_string(const char *name, const char *fallback, char *out, size_t size) {
    (void)name; snprintf(out, size, "%s", fallback);
}
/* Low Power Mode as the system would report it: a notice changes fake_low_power and counts */
static int fake_low_power, fake_limited;
static unsigned long fake_notices;
static double fake_display = 60.0;
void host_low_power_install(void) {}
int host_low_power_enabled(void) { return fake_low_power; }
unsigned long host_low_power_notices(void) { return fake_notices; }
static unsigned fake_output;
double host_low_power_display_rate(unsigned *output) { *output = fake_output; return fake_display; }
int host_low_power_display_limited(void) { return fake_limited; }
static void notice(int on) { fake_low_power = on; fake_notices++; }
/* Presentation timing also runs in shadow-only builds. */
void host_perf_hooks_context(unsigned field, uint64_t value) { (void)field; (void)value; }
void host_perf_hooks_present_begin(void) {}
void host_perf_hooks_present_end(void) {}
void host_external_snapshot(struct host_external_snapshot *out) { memset(out, 0, sizeof(*out)); }
void host_frame_counter_show(unsigned long frame) { (void)frame; }
void host_benchmark_frame(double interval, double cpu, double gpu, double refresh, double simulation, double render,
    double translation, int ticks) {
    (void)interval; (void)cpu; (void)gpu; (void)refresh; (void)simulation; (void)render; (void)translation; (void)ticks;
}
void host_frame_cpu_split(double *simulation, double *render, double *translation, int *ticks) {
    *simulation = *render = *translation = -1.0; *ticks = 0;
}
void host_data_updates_healthy(void) { healthSignals++; [launchStore markHealthy]; }
static uint32_t Present(gpu_texture picture) { (void)picture; assert(healthSignals == 0); return 0; }
static uint32_t PresentStereo(const struct gpu_stereo_present *picture) { (void)picture; assert(healthSignals == 0); return 0; }
static void Context(uint32_t scene, uint32_t rate, const char *level) { (void)scene; (void)rate; (void)level; }
static const struct gpu_backend renderer = {.present = Present, .present_stereo = PresentStereo, .frame_context = Context};

static HostDataPackStore *Store(NSString *root, NSData *key) {
    return [[HostDataPackStore alloc] initWithRoot:root bundled:@"/bundled" kind:@"profiles" channel:@"stable"
        publicKey:key compatibility:@"profiles-2" validator:^BOOL(NSString *folder, NSError **error) {
            (void)error;
            return [NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"test.toml"]];
        }];
}
static NSData *Envelope(uint8_t *secret, int sequence) {
    NSData *payload = [NSJSONSerialization dataWithJSONObject:@{@"files": @{@"test.toml": @"eA=="}} options:0 error:NULL];
    uint8_t hash[CC_SHA256_DIGEST_LENGTH], signature[64];
    CC_SHA256(payload.bytes, (CC_LONG)payload.length, hash);
    NSMutableString *digest = [NSMutableString string];
    for (NSUInteger i = 0; i < sizeof(hash); i++) [digest appendFormat:@"%02x", hash[i]];
    NSData *manifest = [NSJSONSerialization dataWithJSONObject:@{@"schema": @1, @"kind": @"profiles", @"channel": @"stable",
        @"sequence": @(sequence), @"minimum_engine": @1, @"maximum_engine": @1, @"compatibility": @"profiles-2", @"sha256": digest, @"notes": @"test"} options:0 error:NULL];
    crypto_ed25519_sign(signature, secret, manifest.bytes, manifest.length);
    return [NSJSONSerialization dataWithJSONObject:@{@"manifest": [manifest base64EncodedStringWithOptions:0],
        @"signature": [[NSData dataWithBytes:signature length:64] base64EncodedStringWithOptions:0],
        @"payload": [payload base64EncodedStringWithOptions:0]} options:0 error:NULL];
}
static void profile_frame_rate(void) {
    metal_backend = 1;
    host_frame_rate_scene(24);
    host_frame_rate_profile(60);
    assert(host_frame_rate_cap() == 60 && !host_frame_rate_unlimited());
    assert(host_frame_rate_target() == 24);
    frame_rate.next = 123;
    host_frame_rate_profile(60);
    assert(frame_rate.next == 123); /* An unchanged profile does not restart pacing. */
    host_frame_rate_profile(30);
    assert(host_frame_rate_cap() == 30 && frame_rate.next == 0 && frame_rate.interval);
    host_frame_rate_profile(0);
    assert(host_frame_rate_cap() == 0 && !frame_rate.interval && !host_frame_rate_unlimited());
    host_frame_rate_profile(-1);
#if TARGET_OS_MACCATALYST || HALO_NATIVE_MAC
    assert(host_frame_rate_unlimited());
#else
    assert(!host_frame_rate_unlimited());
#endif
    assert(host_frame_rate_cap() == 0 && !frame_rate.interval);
    host_frame_rate_profile(0);
    assert(!host_frame_rate_unlimited());
    host_frame_rate_scene(0);
}
/* The dispatch's own frame rate under Low Power Mode notices: halved between frames, menus at the display's pace,
   restored when it turns off. */
static void low_power_frame_rate(void) {
    metal_backend = 1;
    low_power_install();
    /* a ProMotion iPhone with a profile's "display": 120, then 60 in Low Power Mode, back to 120 */
    fake_display = 120.0; fake_limited = 1;
    host_frame_rate_profile(0);
    low_power_scene(0);
    assert(host_frame_rate_cap() == 0 && host_frame_rate_target() == 0 && !host_frame_rate_low_power());
    notice(1);
    /* (nothing changes inside a frame: only at the latch after a Present) */
    assert(host_frame_rate_target() == 0);
    low_power_frame();
    assert(host_frame_rate_low_power() && host_frame_rate_cap() == 60 && host_frame_rate_target() == 60);
    /* menus keep the display's pace, which Low Power Mode holds at 60 */
    low_power_scene(1);
    assert(host_frame_rate_target() == 60);
    /* a cutscene's 30 stays 30 */
    host_frame_rate_scene(30);
    low_power_scene(2);
    assert(host_frame_rate_target() == 30);
    host_frame_rate_scene(0);
    low_power_scene(0);
    assert(host_frame_rate_target() == 60);
    assert(host_frame_rate_effective() == 60);
    /* a screen that reports Low Power Mode's 60 as its highest still halves its own 120 */
    fake_display = 60.0;
    low_power_update(1);
    assert(host_frame_rate_target() == 60);
    /* a benchmark case's 30 for one run, and back: Low Power Mode's 60 again */
    host_frame_rate_override(30);
    assert(host_frame_rate_target() == 30);
    host_frame_rate_override(0);
    assert(host_frame_rate_target() == 60);
    fake_display = 120.0;
    notice(0);
    low_power_frame();
    assert(!host_frame_rate_low_power() && host_frame_rate_cap() == 0 && host_frame_rate_target() == 0);
    assert(host_frame_rate_effective() == 120);
    /* another output (a 60 Hz external display) while Low Power Mode is on: its own rate */
    notice(1);
    fake_display = 60.0; fake_output++;
    low_power_frame();
    low_power_update(1);
    assert(host_frame_rate_target() == 30);
    notice(0);
    fake_output--;
    low_power_frame();
    /* a 60 Hz iPad with a profile's 60 (no cap there): 30 */
    fake_display = 60.0;
    low_power_update(1);
    notice(1);
    low_power_frame();
    assert(host_frame_rate_cap() == 30 && host_frame_rate_target() == 30);
    low_power_scene(1);
    assert(host_frame_rate_cap() == 0 && host_frame_rate_target() == 0);
    low_power_scene(0);
    /* a profile's 30: stays 30, the game's tick rate */
    host_frame_rate_profile(30);
    assert(host_frame_rate_target() == 30);
    host_frame_rate_profile(0);
    notice(0);
    low_power_frame();
    assert(host_frame_rate_target() == 0);
    /* the Mac app's "unlimited" on a 120 Hz display: 60, held by its sleep, and frames wait for the display */
    fake_display = 120.0; fake_limited = 0;
    low_power_update(1);
    host_frame_rate_profile(-1);
    notice(1);
    low_power_frame();
    assert(!host_frame_rate_unlimited() && host_frame_rate_target() == 60 && low_power.interval);
    notice(0);
    low_power_frame();
    assert(!low_power.interval && host_frame_rate_target() == 0);
    host_frame_rate_profile(0);
    /* debug.low_power_frame_rate = "always" acts without the system's Low Power Mode, "false" ignores it */
    low_power.state.override = LOW_POWER_ALWAYS;
    low_power_frame();
    assert(host_frame_rate_target() == 60);
    low_power.state.override = LOW_POWER_IGNORE;
    notice(1);
    low_power_frame();
    assert(host_frame_rate_target() == 0 && !host_frame_rate_low_power());
    low_power.state.override = LOW_POWER_FOLLOW;
    notice(0);
    low_power_frame();
    fake_display = 60.0;
    low_power_update(1);
}
static HostDataPackStore *Shared(NSString *root, NSData *key) {
    return [HostDataPackStore sharedStoreWithRoot:root bundled:@"/bundled" kind:@"profiles" channel:@"stable"
        publicKey:key compatibility:@"profiles-2" validator:^BOOL(NSString *folder, NSError **error) {
            (void)error;
            return [NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"test.toml"]];
        }];
}
static NSDictionary *State(NSString *root) {
    return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"state.json"]] options:0 error:NULL];
}
static NSArray *StageLeftovers(NSString *root) {
    return [[NSFileManager.defaultManager contentsOfDirectoryAtPath:root error:NULL] filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"SELF BEGINSWITH '.stage-'"]];
}
/* 0.6.3 gave the upscaling check before the launch menu its own store on the menu's root. Launch 1: both downloaded the
   new shard and the second could not move its copy into place. Launch 2: the check began a launch, promoting the shard
   to a trial, which the menu's store then read as a crashed launch and quarantined; every later check said "already
   seen". */
static void shared_store_launches(uint8_t *secret, NSData *key) {
    NSString *root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSError *error = nil;
    // Two consumers on one root get one store, so one download stages and the other is the same sequence.
    [HostDataPackStore forgetSharedStores];
    HostDataPackStore *peek = Shared(root, key), *menu = Shared(root, key);
    assert(peek == menu);
    assert([[[peek menuSnapshot] objectForKey:@"folder"] isEqual:@"/bundled"]);
    assert([[[menu menuSnapshot] objectForKey:@"folder"] isEqual:@"/bundled"]);
    NSData *first = Envelope(secret, 1793231748);
    __block int staged = 0, seen = 0;
    dispatch_group_t group = dispatch_group_create();
    for (HostDataPackStore *consumer in @[peek, menu]) dispatch_group_async(group, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        NSError *stageError = nil;
        BOOL ok = [consumer stageEnvelope:first error:&stageError];
        @synchronized(group) { if (ok) staged++; else if ([stageError.localizedDescription isEqual:@"Pack sequence was already seen"]) seen++; }
    });
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    assert(staged == 1 && seen == 1 && StageLeftovers(root).count == 0);
    NSString *candidate = [menu menuSnapshot][@"folder"];
    assert(![candidate isEqual:@"/bundled"] && [[menu menuSnapshot][@"metadata"][@"sequence"] longLongValue] == 1793231748);
    // The player leaves at the menu. Launch 2: the check before the menu must not begin a launch.
    [HostDataPackStore forgetSharedStores];
    peek = Shared(root, key);
    assert([[peek menuSnapshot][@"folder"] isEqual:candidate]);
    menu = Shared(root, key);
    assert(menu == peek && [[menu menuSnapshot][@"folder"] isEqual:candidate]);
    assert([State(root)[@"quarantined"] longLongValue] == 0 && ![State(root)[@"trial"] boolValue]);
    [menu retainMenuFolder:candidate];
    [menu selectMenuFolder:candidate];
    assert(State(root)[@"trial"] == (id)kCFBooleanTrue);
    [menu markHealthy];
    // Launch 3 keeps it.
    [HostDataPackStore forgetSharedStores];
    assert([[Shared(root, key) menuSnapshot][@"folder"] isEqual:candidate]);
    assert([State(root)[@"quarantined"] longLongValue] == 0 && [State(root)[@"healthy"] length]);
    // A launch without the menu also shares the store the check opened, and its trial is its own.
    [HostDataPackStore forgetSharedStores];
    peek = Shared(root, key);
    assert([peek stageEnvelope:Envelope(secret, 1793231749) error:NULL]);
    [HostDataPackStore forgetSharedStores];
    peek = Shared(root, key);
    NSString *peeked = [peek menuSnapshot][@"folder"];
    HostDataPackStore *legacy = Shared(root, key);
    assert(legacy == peek && [[legacy beginLaunch] isEqual:peeked]);
    [legacy markHealthy];
    [HostDataPackStore forgetSharedStores];
    assert([[Shared(root, key) beginLaunch] isEqual:peeked] && [State(root)[@"quarantined"] longLongValue] == 0);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];

    // A generation already in place (two stores of an older build on one root) is adopted, not an error.
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *one = Store(root, key), *two = Store(root, key);
    assert([one stageEnvelope:first error:NULL]);
    error = nil;
    assert([two stageEnvelope:first error:&error] && !error);
    assert([[two menuSnapshot][@"metadata"][@"sequence"] longLongValue] == 1793231748 && StageLeftovers(root).count == 0);
    // Damaged content under the generation's name is replaced.
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    one = Store(root, key); two = Store(root, key);
    assert([one stageEnvelope:first error:NULL]);
    NSString *staged1 = [one menuSnapshot][@"folder"];
    [@"damaged" writeToFile:[staged1.stringByDeletingLastPathComponent stringByAppendingPathComponent:@"envelope.json"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    assert([two stageEnvelope:first error:NULL]);
    assert([[Store(root, key) menuSnapshot][@"folder"] isEqual:staged1]);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];

    // A device left in the 0.6.3 state (the shard quarantined, its sequence used up) takes the next sequence and keeps it.
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    assert([Store(root, key) stageEnvelope:first error:NULL]);
    assert(![[Store(root, key) beginLaunch] isEqual:@"/bundled"]);                 // 0.6.3's check began a launch
    [HostDataPackStore forgetSharedStores];                                          // (the next process)
    assert([[Store(root, key) menuSnapshot][@"folder"] isEqual:@"/bundled"]);       // and the menu read it as a crash
    assert([State(root)[@"quarantined"] longLongValue] == 1793231748 && [State(root)[@"highest"] longLongValue] == 1793231748);
    [HostDataPackStore forgetSharedStores];
    menu = Shared(root, key);
    assert([[menu menuSnapshot][@"folder"] isEqual:@"/bundled"]);
    error = nil;
    assert(![menu stageEnvelope:first error:&error] && [error.localizedDescription isEqual:@"Pack sequence was already seen"]);
    assert([menu stageEnvelope:Envelope(secret, 1793300000) error:NULL]);
    NSString *recovered = [menu menuSnapshot][@"folder"];
    assert(![recovered isEqual:@"/bundled"]);
    [menu retainMenuFolder:recovered]; [menu selectMenuFolder:recovered]; [menu markHealthy];
    for (int launch = 0; launch < 2; launch++) {
        [HostDataPackStore forgetSharedStores];
        HostDataPackStore *again = Shared(root, key);
        assert([[again menuSnapshot][@"folder"] isEqual:recovered] && Shared(root, key) == again);
        [again retainMenuFolder:recovered]; [again selectMenuFolder:recovered]; [again markHealthy];
        // Choosing the already-healthy snapshot writes a state the next start can read (a boolean trial).
        assert([State(root)[@"quarantined"] longLongValue] == 1793231748 && State(root)[@"trial"] == (id)kCFBooleanFalse);
    }
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];

    // A trial this process began is not a crashed launch, even to another instance on the root.
    [HostDataPackStore forgetSharedStores];
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    assert([Store(root, key) stageEnvelope:first error:NULL]);
    HostDataPackStore *starter = Store(root, key);
    NSString *trialFolder = [starter beginLaunch];
    assert(![trialFolder isEqual:@"/bundled"] && [State(root)[@"trial"] boolValue]);
    assert([[Store(root, key) menuSnapshot][@"folder"] isEqual:trialFolder]);
    assert([State(root)[@"quarantined"] longLongValue] == 0 && [State(root)[@"trial"] boolValue]);
    [starter markHealthy];
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];

    // A store never removes a stage folder this process is filling; one an earlier process left goes.
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *pruner = Store(root, key);
    NSString *ours = [root stringByAppendingPathComponent:[NSString stringWithFormat:@".stage-%d-busy", getpid()]];
    NSString *left = [root stringByAppendingPathComponent:@".stage-0B1D-old"];
    [NSFileManager.defaultManager createDirectoryAtPath:ours withIntermediateDirectories:YES attributes:nil error:NULL];
    [NSFileManager.defaultManager createDirectoryAtPath:left withIntermediateDirectories:YES attributes:nil error:NULL];
    assert([pruner stageEnvelope:first error:NULL]);
    assert([NSFileManager.defaultManager fileExistsAtPath:ours] && ![NSFileManager.defaultManager fileExistsAtPath:left]);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];

    // A sequence marked seen with nothing installed (the 0.6.3 recordings state) is taken again; an envelope refused for
    // its content is not downloaded again, while another at the same sequence still is.
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    [NSFileManager.defaultManager createDirectoryAtPath:root withIntermediateDirectories:YES attributes:nil error:NULL];
    [[NSString stringWithFormat:@"{\"schema\":2,\"kind\":\"profiles\",\"channel\":\"stable\",\"compatibility\":\"profiles-2\",\"highest\":1793178642,\"quarantined\":0,\"active\":\"\",\"healthy\":\"\",\"pending\":\"\",\"trial\":false,\"last_check\":0}"]
        writeToFile:[root stringByAppendingPathComponent:@"state.json"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    HostDataPackStore *stuck = Store(root, key);
    assert([stuck stageEnvelope:Envelope(secret, 1793178642) error:NULL]);
    assert([[stuck menuSnapshot][@"metadata"][@"sequence"] longLongValue] == 1793178642 && [State(root)[@"highest"] longLongValue] == 1793178642);
    error = nil;
    assert(![stuck stageEnvelope:Envelope(secret, 1793178642) error:&error] && [error.localizedDescription isEqual:@"Pack sequence was already seen"]);
    assert(![stuck stageEnvelope:Envelope(secret, 1793178000) error:NULL]);   // older than what is installed
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *refusing = [HostDataPackStore sharedStoreWithRoot:root bundled:@"/bundled" kind:@"profiles" channel:@"stable"
        publicKey:key compatibility:@"profiles-2" validator:^BOOL(NSString *folder, NSError **validationError) { (void)folder; (void)validationError; return NO; }];
    assert(![refusing stageEnvelope:Envelope(secret, 7) error:NULL]);
    error = nil;
    assert(![refusing stageEnvelope:Envelope(secret, 7) error:&error] && [error.localizedDescription isEqual:@"Pack sequence was already seen"]);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];

    // A look before the launch (the upscaling gate) reads what a launch starting now would use and changes nothing.
    HostPackValidator hasTest = ^BOOL(NSString *folder, NSError **validationError) {
        (void)validationError; return [NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"test.toml"]];
    };
    NSString *(^Peek)(NSString *) = ^NSString *(NSString *at) {
        return [HostDataPackStore peekFolderAtRoot:at bundled:@"/bundled" kind:@"profiles" channel:@"stable" publicKey:key compatibility:@"profiles-2" validator:hasTest];
    };
    root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    assert([Peek(root) isEqual:@"/bundled"] && ![NSFileManager.defaultManager fileExistsAtPath:root]);   // nothing created
    [HostDataPackStore forgetSharedStores];
    HostDataPackStore *looked = Store(root, key);
    assert([looked stageEnvelope:Envelope(secret, 40) error:NULL]);
    NSString *candidate40 = [looked menuSnapshot][@"folder"];
    NSData *stateBytes = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"state.json"]];
    assert([Peek(root) isEqual:candidate40]);                                                    // the pending shard
    assert([[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"state.json"]] isEqual:stateBytes]);
    [looked selectMenuFolder:candidate40]; [looked markHealthy];
    assert([looked stageEnvelope:Envelope(secret, 41) error:NULL]);
    [HostDataPackStore forgetSharedStores];
    NSString *trial41 = [Store(root, key) beginLaunch];                                          // a launch that never got healthy
    [HostDataPackStore forgetSharedStores];
    assert(![trial41 isEqual:candidate40] && [Peek(root) isEqual:candidate40]);                // its trial counts as rolled back
    stateBytes = [NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"state.json"]];
    assert([State(root)[@"trial"] boolValue] && [[NSData dataWithContentsOfFile:[root stringByAppendingPathComponent:@"state.json"]] isEqual:stateBytes]);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    [HostDataPackStore forgetSharedStores];
    puts("PASS: one store per root; launch 1 then launch 2 keep a new shard; a 0.6.3 quarantine yields to the next sequence");
}
#define CATALOG_GUEST @"benchmark-catalog-1:c2ce7f3b23742865e51ad2716fd5dd15ecfe14c68c205cea5ba5be33e3d00b89"
static HostDataPackStore *RecordingStore(NSString *root, NSData *key, BOOL shared) {
    BOOL (^validator)(NSString *, NSError **) = ^BOOL(NSString *folder, NSError **error) {
        (void)error;
        return [NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"a10-test.input"]];
    };
    return shared ? [HostDataPackStore sharedStoreWithRoot:root bundled:@"/bundled" kind:@"recordings" channel:@"tester"
                        publicKey:key compatibility:CATALOG_GUEST validator:validator]
                  : [[HostDataPackStore alloc] initWithRoot:root bundled:@"/bundled" kind:@"recordings" channel:@"tester"
                        publicKey:key compatibility:CATALOG_GUEST validator:validator];
}
static NSData *RecordingEnvelope(uint8_t *secret, long long sequence) {
    NSData *payload = [NSJSONSerialization dataWithJSONObject:@{@"files": @{@"a10-test.input": @"eA=="}} options:0 error:NULL];
    uint8_t hash[CC_SHA256_DIGEST_LENGTH], signature[64];
    CC_SHA256(payload.bytes, (CC_LONG)payload.length, hash);
    NSMutableString *digest = [NSMutableString string];
    for (NSUInteger i = 0; i < sizeof(hash); i++) [digest appendFormat:@"%02x", hash[i]];
    NSData *manifest = [NSJSONSerialization dataWithJSONObject:@{@"schema": @1, @"kind": @"recordings", @"channel": @"tester",
        @"sequence": @(sequence), @"minimum_engine": @1, @"maximum_engine": @1, @"compatibility": CATALOG_GUEST, @"sha256": digest, @"notes": @"test"} options:0 error:NULL];
    crypto_ed25519_sign(signature, secret, manifest.bytes, manifest.length);
    return [NSJSONSerialization dataWithJSONObject:@{@"manifest": [manifest base64EncodedStringWithOptions:0],
        @"signature": [[NSData dataWithBytes:signature length:64] base64EncodedStringWithOptions:0],
        @"payload": [payload base64EncodedStringWithOptions:0]} options:0 error:NULL];
}
/* 0.6.4 moved only the profiles to a new cache root; the recordings store keeps 0.6.3's state. 0.6.3's two stores on the
   recordings root quarantined the catalog's trial as they did a profile shard's (the legacy store began the launch, the
   menu's store read it as a crash), so its sequence is used up: the same catalog is refused for good, and only the
   catalog signed again at a higher sequence stages and stays. */
static void recordings_quarantined_by_063(uint8_t *secret, NSData *key) {
    [HostDataPackStore forgetSharedStores];
    NSString *root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSData *live = RecordingEnvelope(secret, 1793178642);
    assert([RecordingStore(root, key, NO) stageEnvelope:live error:NULL]);
    assert(![[RecordingStore(root, key, NO) beginLaunch] isEqual:@"/bundled"]);      // 0.6.3's legacy store began a launch
    [HostDataPackStore forgetSharedStores];                                           // (the next process)
    assert([[RecordingStore(root, key, NO) menuSnapshot][@"folder"] isEqual:@"/bundled"]);
    assert([State(root)[@"quarantined"] longLongValue] == 1793178642);
    // 0.6.4, one store per root: the live catalog is refused at every check, so Benchmark stays unavailable
    for (int launch = 0; launch < 2; launch++) {
        [HostDataPackStore forgetSharedStores];
        HostDataPackStore *menu = RecordingStore(root, key, YES);
        NSError *error = nil;
        assert(![menu stageEnvelope:live error:&error] && [error.localizedDescription isEqual:@"Pack sequence was already seen"]);
        assert([[menu menuSnapshot][@"folder"] isEqual:@"/bundled"]);
    }
    // the same catalog signed again at a higher sequence stages, is chosen and stays healthy
    [HostDataPackStore forgetSharedStores];
    HostDataPackStore *menu = RecordingStore(root, key, YES);
    assert([menu stageEnvelope:RecordingEnvelope(secret, 1793290000) error:NULL]);
    NSString *recovered = [menu menuSnapshot][@"folder"];
    assert(![recovered isEqual:@"/bundled"]);
    [menu retainMenuFolder:recovered]; [menu selectMenuFolder:recovered]; [menu markHealthy];
    [HostDataPackStore forgetSharedStores];
    assert([[RecordingStore(root, key, YES) menuSnapshot][@"folder"] isEqual:recovered]);
    // a fresh install (no 0.6.3 state) takes the live catalog at once
    [HostDataPackStore forgetSharedStores];
    NSString *fresh = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *first = RecordingStore(fresh, key, YES);
    assert([first stageEnvelope:live error:NULL] && ![[first menuSnapshot][@"folder"] isEqual:@"/bundled"]);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    [NSFileManager.defaultManager removeItemAtPath:fresh error:NULL];
    [HostDataPackStore forgetSharedStores];
    puts("PASS: a recordings catalog 0.6.3 quarantined is refused; the catalog at a higher sequence is taken; a fresh install takes the live one");
}
int main(void) { @autoreleasepool {
    profile_frame_rate();
    low_power_frame_rate();
    uint8_t seed[32] = {2}, secret[64], public[32]; // Public fixture, never a release key.
    crypto_ed25519_key_pair(secret, public, seed);
    NSData *key = [NSData dataWithBytes:public length:32], *envelope = Envelope(secret, 1);
    shared_store_launches(secret, key);
    recordings_quarantined_by_063(secret, key);
    // Menu candidates stay pending until selected; abandoned menus do not quarantine them.
    NSString *menuRoot = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *menu = Store(menuRoot, key);
    [menu beginLaunch];
    assert([menu stageEnvelope:envelope error:NULL]);
    NSDictionary *candidate = [menu menuSnapshot];
    assert([candidate[@"metadata"][@"sequence"] intValue] == 1);
    NSDictionary *reopened = [Store(menuRoot, key) menuSnapshot];
    assert([reopened[@"folder"] isEqual:candidate[@"folder"]]);
    // A later staged generation must not prune the candidate the user just selected.
    assert([menu stageEnvelope:Envelope(secret, 2) error:NULL]);
    assert([NSFileManager.defaultManager fileExistsAtPath:candidate[@"folder"]]);
    NSDictionary *superseded = [menu menuSnapshot];
    [menu retainMenuFolder:candidate[@"folder"]];
    assert([menu stageEnvelope:Envelope(secret, 3) error:NULL]);
    assert(![NSFileManager.defaultManager fileExistsAtPath:superseded[@"folder"]]);
    assert([NSFileManager.defaultManager fileExistsAtPath:candidate[@"folder"]]);
    [menu selectMenuFolder:candidate[@"folder"]];
    assert([[menu metadata][@"sequence"] intValue] == 1);
    [menu markHealthy];
    HostDataPackStore *nextMenu = Store(menuRoot, key);
    assert(![[nextMenu beginLaunch] isEqual:candidate[@"folder"]]);
    [NSFileManager.defaultManager removeItemAtPath:menuRoot error:NULL];
    // Choosing the snapshot that is already healthy keeps a state the next start can read: trial is a boolean.
    NSString *healthyRoot = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *chooser = Store(healthyRoot, key);
    assert([chooser stageEnvelope:envelope error:NULL]);
    NSString *chosen = [chooser menuSnapshot][@"folder"];
    [chooser selectMenuFolder:chosen]; [chooser markHealthy];
    chooser = Store(healthyRoot, key);
    assert([[chooser menuSnapshot][@"folder"] isEqual:chosen]);
    [chooser selectMenuFolder:chosen];
    NSDictionary *chosenState = [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[healthyRoot stringByAppendingPathComponent:@"state.json"]] options:0 error:NULL];
    assert(chosenState[@"trial"] == (id)kCFBooleanFalse);
    assert([[Store(healthyRoot, key) menuSnapshot][@"folder"] isEqual:chosen]);
    [NSFileManager.defaultManager removeItemAtPath:healthyRoot error:NULL];
    NSString *invalidRoot = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *invalidMenu = Store(invalidRoot, key);
    assert([invalidMenu stageEnvelope:envelope error:NULL]);
    NSDictionary *invalidCandidate = [invalidMenu menuSnapshot];
    [@"corrupted" writeToFile:[invalidCandidate[@"folder"] stringByAppendingPathComponent:@"test.toml"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    assert([[[invalidMenu menuSnapshot] objectForKey:@"folder"] isEqual:@"/bundled"]);
    NSDictionary *invalidState = [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[invalidRoot stringByAppendingPathComponent:@"state.json"]] options:0 error:NULL];
    assert([invalidState[@"quarantined"] intValue] == 1 && ![invalidState[@"pending"] length]);
    [NSFileManager.defaultManager removeItemAtPath:invalidRoot error:NULL];
    for (int mode = 0; mode < 7; mode++) {
        /* Menu, direct gameplay, opening cutscene/stereo, loading-only,
           failure before rendering, failure before presentation, and a loading frame after an abandoned world frame. */
        const char *levels[] = {"ui", "b30", "a10", "b30", "", "b30", "b30"};
        NSString *root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        launchStore = Store(root, key);
        assert([launchStore stageEnvelope:envelope error:NULL]);
        NSString *trial = [launchStore beginLaunch];
        updates_healthy = 0;
        ready_frame = 0;
        healthSignals = 0;
        wrapped_backend = &renderer;
        backend = &renderer;
        metal_backend = 1;
        gpu_frame_context(mode == 0 ? 0 : mode == 2 ? 1 : 2, 0, levels[mode]);
        /* Startup and loading frames do not qualify, even with a named level. */
        present_wrapped(1);
        present_stereo_wrapped(NULL);
        assert(healthSignals == 0);
        if (mode < 3 || mode >= 5) host_data_updates_frame_ready(1);
        assert(healthSignals == 0); // Rendering alone does not accept the pack.
        if (mode == 6) {
            host_data_updates_frame_ready(0);
            present_wrapped(1);
            present_stereo_wrapped(NULL);
            assert(healthSignals == 0);
        }
        [HostDataPackStore forgetSharedStores];   // the stores below are the next process's
        if (mode < 3) {
            if (mode == 2) present_stereo_wrapped(NULL); else present_wrapped(1);
            assert(healthSignals == 1);
            assert([[Store(root, key) beginLaunch] isEqual:trial]);
        } else {
            assert([[Store(root, key) beginLaunch] isEqual:@"/bundled"]);
            assert(![Store(root, key) stageEnvelope:envelope error:NULL]);   // quarantined, not taken again
        }
        [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    }
    puts("PASS: menu, gameplay and stereo retain signed packs; loading and failed launches roll back");
} return 0; }
