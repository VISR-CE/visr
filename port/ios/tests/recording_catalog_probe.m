/* A published benchmark-catalog-2 feed and its archive, read through the real store and the real catalog validator:
 * argv: FEED.json BLOB.xz ORIGINAL_FILES_DIR PUBLIC_KEY_HEX GUEST_SHA256 EXPECTED_CASES.
 * The expanded folder must hold exactly the catalog-1 files, byte for byte, and load as the expected number of cases. */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include "../host/host_benchmark_catalog.h"
#include "../host/host_data_pack.h"
#include <stdarg.h>
void host_logf(int priority, const char *format, ...) { (void)priority; (void)format; }
static NSData *feedData, *blobData;
@interface CatalogProtocol : NSURLProtocol
@end
@implementation CatalogProtocol
+ (BOOL)canInitWithRequest:(NSURLRequest *)request { (void)request; return YES; }
+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request { return request; }
- (void)startLoading {
    NSData *data = [self.request.URL.path hasSuffix:@".xz"] ? blobData : [self.request.URL.path hasSuffix:@".json"] ? feedData : nil;
    NSHTTPURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:self.request.URL statusCode:data ? 200 : 404 HTTPVersion:@"HTTP/1.1" headerFields:@{}];
    [self.client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    if (data) [self.client URLProtocol:self didLoadData:data];
    [self.client URLProtocolDidFinishLoading:self];
}
- (void)stopLoading {}
@end
static IMP original;
static NSURLSessionConfiguration *Configuration(id object, SEL selector) {
    NSURLSessionConfiguration *configuration = ((id(*)(id, SEL))original)(object, selector);
    configuration.protocolClasses = @[CatalogProtocol.class]; return configuration;
}
static NSData *HexData(NSString *hex) {
    NSMutableData *data = [NSMutableData data];
    for (NSUInteger i = 0; i + 1 < hex.length; i += 2) { unsigned value; sscanf([hex substringWithRange:NSMakeRange(i, 2)].UTF8String, "%2x", &value); uint8_t byte = value; [data appendBytes:&byte length:1]; }
    return data;
}
int main(int argc, char **argv) { @autoreleasepool {
    if (argc != 7) return 2;
    feedData = [NSData dataWithContentsOfFile:@(argv[1])]; blobData = [NSData dataWithContentsOfFile:@(argv[2])];
    NSString *originals = @(argv[3]), *guest = @(argv[5]);
    NSInteger expectedCases = atoi(argv[6]);
    if (!feedData.length || !blobData.length) { fprintf(stderr, "FAIL: fixture files are unreadable\n"); return 1; }
    Method method = class_getClassMethod(NSURLSessionConfiguration.class, @selector(ephemeralSessionConfiguration));
    original = method_setImplementation(method, (IMP)Configuration);
    NSString *root = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    HostDataPackStore *(^open)(void) = ^{
        return [[HostDataPackStore alloc] initWithRoot:root bundled:@"/bundled" kind:@"recordings" channel:@"tester" publicKey:HexData(@(argv[4])) compatibility:[@"benchmark-catalog-2:" stringByAppendingString:guest] validator:^BOOL(NSString *folder, NSError **error) {
            return host_benchmark_catalog_validate_folder(folder, error);
        }];
    };
    HostDataPackStore *store = open(); [store beginLaunch];
    [store checkForUpdateAtURL:[NSURL URLWithString:@"https://feed.invalid/packs/tester/recordings-2-x.json"]];
    for (int i = 0; i < 3000 && [store valueForKey:@"session"]; i++) [NSThread sleepForTimeInterval:0.01];
    store = open(); NSString *folder = [store beginLaunch];
    if ([[store metadata][@"bundled"] boolValue]) { fprintf(stderr, "FAIL: the published feed did not activate\n"); return 1; }
    struct host_benchmark_catalog catalog; NSError *error = nil;
    if (!host_benchmark_catalog_load(folder, &catalog, &error)) { fprintf(stderr, "FAIL: catalog load: %s\n", error.localizedDescription.UTF8String); return 1; }
    int failures = 0;
    if (catalog.count != expectedCases) { fprintf(stderr, "FAIL: %d cases, expected %ld\n", catalog.count, (long)expectedCases); failures++; }
    NSArray *names = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:originals error:NULL] sortedArrayUsingSelector:@selector(compare:)];
    NSArray *expanded = [[NSFileManager.defaultManager contentsOfDirectoryAtPath:folder error:NULL] sortedArrayUsingSelector:@selector(compare:)];
    if (![names isEqual:expanded]) { fprintf(stderr, "FAIL: file names differ from the catalog-1 files\n"); failures++; }
    for (NSString *name in names) if (![[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:name]] isEqual:[NSData dataWithContentsOfFile:[originals stringByAppendingPathComponent:name]]]) { fprintf(stderr, "FAIL: %s differs\n", name.UTF8String); failures++; }
    int replayable = 0;
    for (int i = 0; i < catalog.count; i++) {
        if (!catalog.recordings[i][0]) continue;
        BOOL complete = YES;
        for (NSString *extension in @[@"input", @"actions", @"seeds", @"route"])
            complete &= [NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.%@", catalog.recordings[i], extension]]];
        if (!complete) { fprintf(stderr, "FAIL: case %s lacks recording files\n", catalog.ids[i]); failures++; } else replayable++;
    }
    printf("catalog-2 feed: %d cases, %lu files byte-identical to the catalog-1 payload, %d cases with complete recordings\n", catalog.count, (unsigned long)names.count, replayable);
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    return failures ? 1 : 0;
} }
