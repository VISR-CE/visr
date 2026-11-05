/* A build's feeds read from feeds.visr.gg (docs/data-packs.md, "Feed hosts"): each store fetches its feed, every file
   it names and the latest-release record from a local server standing in for the host, checks the signature with the
   build's key and the payload with the app's own validator, and activates it on the next launch. Any request for
   another host or scheme fails the probe. tools/feeds_host_check.py stages the bucket's tree, serves it and runs this.
   Usage: feeds-host-probe HOST PORT PUBLIC_KEY PROFILE_FEED_URL SHADER_FEED_URL RECORDING_FEED_URL RECORDING_COMPATIBILITY */
#import "../host/host_data_pack.h"
#include "../host/host.h"
#include "../host/host_profile.h"
#include "../host/host_recording_pack.h"
#include "../host/host_shader_pack.h"
#include <stdarg.h>
#import <objc/runtime.h>

void host_logf(int priority, const char *format, ...)
{
    (void)priority; va_list arguments; va_start(arguments, format); vfprintf(stderr, format, arguments); va_end(arguments); fputc('\n', stderr);
}
static struct host_device phone(void)
{
    struct host_device d; memset(&d, 0, sizeof(d));
    strcpy(d.platform, "ios"); strcpy(d.model, "iPhone18,1");
    d.gpu_family = 10; d.memory_megabytes = 12000; d.pixel_width = 2622; d.pixel_height = 1206; d.refresh_rate = 120;
    return d;
}
void host_device_identify(struct host_device *d) { *d = phone(); }

static int failures;
#define CHECK(condition, label) do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", label); failures++; } else printf("ok: %s\n", label); } while (0)

static NSString *feedHost, *localBase;
static NSMutableArray<NSString *> *requested, *foreign;

/* Every store request lands here. Requests for the feed host are answered by the local server, status, validator and
   body unchanged; anything else is recorded and fails, so a URL that strays from the host cannot pass unnoticed. */
@interface FeedsHost : NSURLProtocol
@end
@implementation FeedsHost
+ (BOOL)canInitWithRequest:(NSURLRequest *)request { (void)request; return YES; }
+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request { return request; }
- (void)startLoading {
    NSURL *url = self.request.URL;
    if (![url.scheme isEqual:@"https"] || ![url.host isEqual:feedHost] || url.port) {
        @synchronized(foreign) { [foreign addObject:url.absoluteString]; }
        [self.client URLProtocol:self didFailWithError:[NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorCannotFindHost userInfo:nil]];
        return;
    }
    NSString *path = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO].percentEncodedPath;
    @synchronized(requested) { [requested addObject:path]; }
    NSMutableURLRequest *local = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[localBase stringByAppendingString:path]]];
    NSString *validator = [self.request valueForHTTPHeaderField:@"If-None-Match"];
    if (validator) [local setValue:validator forHTTPHeaderField:@"If-None-Match"];
    __block NSData *body; __block NSHTTPURLResponse *answer; __block NSError *failure;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [[NSURLSession.sharedSession dataTaskWithRequest:local completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        body = data; answer = (NSHTTPURLResponse *)response; failure = error; dispatch_semaphore_signal(done);
    }] resume];
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    if (failure || !answer) { [self.client URLProtocol:self didFailWithError:failure ?: [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorUnknown userInfo:nil]]; return; }
    NSHTTPURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:url statusCode:answer.statusCode HTTPVersion:@"HTTP/1.1" headerFields:answer.allHeaderFields];
    [self.client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    if (body.length) [self.client URLProtocol:self didLoadData:body];
    [self.client URLProtocolDidFinishLoading:self];
}
- (void)stopLoading {}
@end
static IMP originalEphemeral;
static NSURLSessionConfiguration *throughFeedsHost(id object, SEL selector)
{
    NSURLSessionConfiguration *configuration = ((NSURLSessionConfiguration *(*)(id, SEL))originalEphemeral)(object, selector);
    configuration.protocolClasses = @[FeedsHost.class];
    return configuration;
}

@interface HostDataPackStore (Probe)
+ (void)forgetSharedStores;
@end
static NSData *key;
static NSString *base, *emptyBundle, *caches = @"Caches";
static HostDataPackStore *makeStore(NSString *kind, NSString *compatibility, HostPackValidator validator)
{
    [HostDataPackStore forgetSharedStores];
    NSString *root = [[base stringByAppendingPathComponent:caches] stringByAppendingPathComponent:kind];
    return [[HostDataPackStore alloc] initWithRoot:root bundled:emptyBundle kind:kind channel:@"tester" publicKey:key compatibility:compatibility validator:validator];
}
/* One launch checks the feed; the next launch uses what it staged, which the store activates only after the signature,
   every named file's hash and the validator have passed. Returns that folder, or nil. */
static NSString *fetched(NSString *kind, NSString *compatibility, NSURL *url, HostPackValidator validator)
{
    HostDataPackStore *store = makeStore(kind, compatibility, validator);
    [store beginLaunch];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block BOOL succeeded = NO;
    [store checkForUpdateAtURL:url completion:^(BOOL success) { succeeded = success; dispatch_semaphore_signal(done); }];
    if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 300 * NSEC_PER_SEC))) return nil;
    printf("%s: %s answered %ld\n", kind.UTF8String, url.absoluteString.UTF8String, (long)store.lastFeedStatus);
    if (!succeeded || store.lastFeedStatus != 200) return nil;
    HostDataPackStore *next = makeStore(kind, compatibility, validator);
    NSString *folder = [next beginLaunch];
    [next markHealthy];
    return [folder isEqual:emptyBundle] || [[next metadata][@"sequence"] longLongValue] < 1 ? nil : folder;
}
static NSData *hex(const char *text)
{
    NSMutableData *data = [NSMutableData data];
    for (size_t i = 0; text[i] && text[i + 1]; i += 2) { unsigned value; sscanf(text + i, "%2x", &value); uint8_t byte = value; [data appendBytes:&byte length:1]; }
    return data;
}
static NSInteger requestsUnder(NSString *prefix)
{
    NSInteger count = 0;
    @synchronized(requested) { for (NSString *path in requested) if ([path hasPrefix:prefix]) count++; }
    return count;
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc != 8) { fprintf(stderr, "usage: %s HOST PORT PUBLIC_KEY PROFILE_FEED_URL SHADER_FEED_URL RECORDING_FEED_URL RECORDING_COMPATIBILITY\n", argv[0]); return 2; }
        feedHost = @(argv[1]); localBase = [NSString stringWithFormat:@"http://127.0.0.1:%s", argv[2]]; key = hex(argv[3]);
        NSURL *profileBase = [NSURL URLWithString:@(argv[4])], *shaders = [NSURL URLWithString:@(argv[5])], *recordings = [NSURL URLWithString:@(argv[6])];
        NSString *recordingCompatibility = @(argv[7]);
        requested = [NSMutableArray array]; foreign = [NSMutableArray array];
        CHECK(key.length == 32, "the build's data-pack key is 32 bytes");
        base = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        emptyBundle = [base stringByAppendingPathComponent:@"bundled"];
        assert([NSFileManager.defaultManager createDirectoryAtPath:emptyBundle withIntermediateDirectories:YES attributes:nil error:NULL]);
        Method ephemeral = class_getClassMethod(NSURLSessionConfiguration.class, @selector(ephemeralSessionConfiguration));
        originalEphemeral = method_setImplementation(ephemeral, (IMP)throughFeedsHost);

        // The profile shard for an iPhone 17 Pro, at the URL the app builds from the profile feed directory.
        struct host_device device = phone(); char deviceKey[192];
        assert(host_profile_device_key(&device, deviceKey, sizeof(deviceKey)));
        NSURL *profile = profileBase;
        for (NSString *part in [@(deviceKey) componentsSeparatedByString:@"/"]) profile = [profile URLByAppendingPathComponent:part];
        profile = [profile URLByAppendingPathExtension:@"json"];
        NSString *profileFolder = fetched(@"profiles", [@HOST_PROFILE_FEED_DOMAIN ":" stringByAppendingString:@(deviceKey)], profile, ^BOOL(NSString *folder, NSError **error) {
            (void)error; return host_profile_validate_feed_folder(folder.fileSystemRepresentation, &device, HOST_PROFILE_FEED_GENERATION, NULL);
        });
        CHECK(profileFolder != nil, "the iPhone 17 Pro's device-profile shard downloads, verifies and activates");

        NSString *shaderFolder = fetched(@"shaders", @HOST_SHADER_PACK_COMPATIBILITY, shaders, ^BOOL(NSString *folder, NSError **error) {
            (void)error; return host_shader_pack_validate(folder.fileSystemRepresentation, NULL, NULL);
        });
        NSString *maps = [[shaders.path stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"shader-maps/"];
        CHECK(shaderFolder != nil, "the shader index downloads, verifies and activates");
        CHECK(requestsUnder(maps) > 0, "every shader map the index names comes from the same host");

        NSString *recordingFolder = fetched(@"recordings", recordingCompatibility, recordings, ^BOOL(NSString *folder, NSError **error) {
            (void)error; return host_recording_pack_validate(folder.fileSystemRepresentation, NULL);
        });
        CHECK(recordingFolder != nil, "the release's recording catalog downloads, verifies and activates");
        if ([recordingCompatibility hasPrefix:@"benchmark-catalog-2:"]) {
            NSString *blobs = [[recordings.path stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"recording-blobs/"];
            CHECK(requestsUnder(blobs) == 1, "its compressed archive comes from the same host");
        }

        // Beside the recordings, as the app derives it.
        NSURL *latest = [recordings.URLByDeletingLastPathComponent URLByAppendingPathComponent:@"latest.json"];
        NSString *latestFolder = fetched(@"latest", @"latest-release-1", latest, ^BOOL(NSString *folder, NSError **error) {
            (void)error;
            NSData *data = [NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"latest-release.json"]];
            NSDictionary *record = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
            return [record isKindOfClass:NSDictionary.class] && [record[@"platforms"] isKindOfClass:NSDictionary.class];
        });
        CHECK(latestFolder != nil, "the latest-release record downloads, verifies and activates");

        // The same served feeds under another key: a build that does not hold the signing key's pair takes nothing.
        NSData *rightKey = key; NSMutableData *wrongKey = [key mutableCopy];
        ((uint8_t *)wrongKey.mutableBytes)[0] ^= 1;
        key = wrongKey; caches = @"Caches-wrong-key";
        CHECK(!fetched(@"shaders", @HOST_SHADER_PACK_COMPATIBILITY, shaders, ^BOOL(NSString *folder, NSError **error) {
            (void)error; return host_shader_pack_validate(folder.fileSystemRepresentation, NULL, NULL);
        }), "the shader index is refused under a different key");
        CHECK(!fetched(@"latest", @"latest-release-1", latest, ^BOOL(NSString *folder, NSError **error) {
            (void)folder; (void)error; return YES;
        }), "the latest-release record is refused under a different key");
        key = rightKey; caches = @"Caches";

        CHECK(foreign.count == 0, "no request leaves the feed host");
        for (NSString *url in foreign) fprintf(stderr, "  request for another host: %s\n", url.UTF8String);
        printf("requests to %s: %lu\n", feedHost.UTF8String, (unsigned long)requested.count);
        method_setImplementation(ephemeral, originalEphemeral);
        [NSFileManager.defaultManager removeItemAtPath:base error:NULL];
    }
    if (failures) { fprintf(stderr, "%d feed host checks failed\n", failures); return 1; }
    puts("PASS: every feed reads, verifies and activates from the feed host");
    return 0;
}
