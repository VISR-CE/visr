/* Exercise signed benchmark-catalog-2 acquisition through the real store: one xz archive, verified by hash, expanded once.
 * argv[1] is an archive made by tools/data_pack.py (Python's lzma); archives built here use Apple's own encoder. */
#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>
#import <objc/runtime.h>
#include <compression.h>
#include "../host/host_data_pack.h"
#include "../../third_party/monocypher/monocypher-ed25519.h"
#include <stdarg.h>
void host_logf(int priority, const char *format, ...) { (void)priority; (void)format; }
static int failures;
#define CHECK(c, label) do { if (!(c)) { fprintf(stderr,"FAIL: %s\n",label); failures++; } } while (0)
static uint8_t secret[64], public[32];
static NSMutableDictionary *responses, *counts;
static NSString *const Guest = @"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static NSData *JSON(id object) { return [NSJSONSerialization dataWithJSONObject:object options:0 error:NULL]; }
static NSData *Bytes(NSString *text) { return [text dataUsingEncoding:NSUTF8StringEncoding]; }
static NSString *SHA(NSData *data) {
    unsigned char bytes[32]; CC_SHA256(data.bytes,(CC_LONG)data.length,bytes);
    NSMutableString *s = [NSMutableString string];
    for (int i=0;i<32;i++) [s appendFormat:@"%02x",bytes[i]];
    return s;
}
static NSData *Archive(NSDictionary *files) {
    NSMutableData *out = [Bytes(@"visr-recordings-1\n") mutableCopy];
    for (NSString *name in [files.allKeys sortedArrayUsingSelector:@selector(compare:)]) {
        [out appendData:Bytes([NSString stringWithFormat:@"%@ %lu\n",name,(unsigned long)[files[name] length]])];
        [out appendData:files[name]]; [out appendData:Bytes(@"\n")];
    }
    return out;
}
static NSData *XZ(NSData *raw) {
    NSMutableData *out = [NSMutableData dataWithLength:raw.length + 1024];
    size_t n = compression_encode_buffer(out.mutableBytes,out.length,raw.bytes,raw.length,NULL,COMPRESSION_LZMA);
    if (!n) abort();
    out.length = n; return out;
}
static NSData *SignedIndex(NSInteger sequence, NSString *descriptor, NSString *compatibility) {
    NSData *payload=JSON(@{@"files":@{@"recordings":descriptor}});
    NSData *manifest=JSON(@{@"schema":@1,@"kind":@"recordings",@"channel":@"tester",@"sequence":@(sequence),@"minimum_engine":@1,@"maximum_engine":@1,@"compatibility":compatibility,@"sha256":SHA(payload),@"notes":@"probe"});
    uint8_t signature[64]; crypto_ed25519_sign(signature,secret,manifest.bytes,manifest.length);
    return JSON(@{@"manifest":[manifest base64EncodedStringWithOptions:0],@"signature":[[NSData dataWithBytes:signature length:64] base64EncodedStringWithOptions:0],@"payload":[payload base64EncodedStringWithOptions:0]});
}
/* Serve compressed under its hash and return the signed index for it; declaredRaw lets a test lie about the size. */
static NSData *Index(NSInteger sequence, NSData *raw, NSData *compressed, NSInteger declaredRaw) {
    NSString *hash=SHA(compressed);
    responses[[[@"/packs/recording-blobs/" stringByAppendingString:hash] stringByAppendingString:@".xz"]]=compressed;
    NSString *descriptor=[NSString stringWithFormat:@"%@:%ld:%@:%lu",SHA(raw),(long)(declaredRaw?declaredRaw:(NSInteger)raw.length),hash,(unsigned long)compressed.length];
    return SignedIndex(sequence,descriptor,[@"benchmark-catalog-2:" stringByAppendingString:Guest]);
}
@interface RecordingProtocol : NSURLProtocol
@end
@implementation RecordingProtocol
+ (BOOL)canInitWithRequest:(NSURLRequest *)request { (void)request; return YES; }
+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request { return request; }
- (void)startLoading {
    NSString *path=self.request.URL.path;
    @synchronized(counts) { counts[path]=@([counts[path] integerValue]+1); }
    NSData *data=responses[path];
    NSHTTPURLResponse *response=[[NSHTTPURLResponse alloc] initWithURL:self.request.URL statusCode:data?200:404 HTTPVersion:@"HTTP/1.1" headerFields:@{}];
    [self.client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    if (data) for (NSUInteger offset=0;offset<data.length;offset+=4096)
        [self.client URLProtocol:self didLoadData:[data subdataWithRange:NSMakeRange(offset,MIN(4096,data.length-offset))]];
    [self.client URLProtocolDidFinishLoading:self];
}
- (void)stopLoading {}
@end
static IMP original;
static NSURLSessionConfiguration *Configuration(id object,SEL selector) {
    NSURLSessionConfiguration *configuration=((id(*)(id,SEL))original)(object,selector);
    configuration.protocolClasses=@[RecordingProtocol.class]; return configuration;
}
static HostDataPackStore *Store(NSString *root, NSString *compatibility) {
    return [[HostDataPackStore alloc] initWithRoot:root bundled:@"/bundled" kind:@"recordings" channel:@"tester" publicKey:[NSData dataWithBytes:public length:32] compatibility:compatibility validator:^BOOL(NSString *folder, NSError **error) {
        (void)error; return [NSFileManager.defaultManager fileExistsAtPath:[folder stringByAppendingPathComponent:@"a.input"]];
    }];
}
static void Update(HostDataPackStore *store,NSData *index) {
    responses[@"/packs/recordings-x.json"]=index;
    [store checkForUpdateAtURL:[NSURL URLWithString:@"https://feed.invalid/packs/recordings-x.json"]];
    for (int i=0;i<1000 && [store valueForKey:@"session"];i++) [NSThread sleepForTimeInterval:0.01];
    CHECK(![store valueForKey:@"session"],"bounded acquisition finishes");
}
static NSInteger BlobRequests(void) {
    NSInteger n=0; @synchronized(counts) { for (NSString *path in counts) if ([path containsString:@"recording-blobs"]) n+=[counts[path] integerValue]; } return n;
}
int main(int argc, char **argv) { @autoreleasepool {
    uint8_t seed[32]={9}; crypto_ed25519_key_pair(secret,public,seed);
    responses=[NSMutableDictionary dictionary]; counts=[NSMutableDictionary dictionary];
    Method method=class_getClassMethod(NSURLSessionConfiguration.class,@selector(ephemeralSessionConfiguration));
    original=method_setImplementation(method,(IMP)Configuration);
    NSString *compatibility=[@"benchmark-catalog-2:" stringByAppendingString:Guest];
    NSString *root=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSDictionary *files=@{@"a.input":Bytes(@"input bytes\nline two\n"),@"a.actions":Bytes(@"actions\n"),@"a.seeds":Bytes(@"1 2 3\n"),@"a.route":Bytes(@"scene = a10\n")};
    NSData *raw=Archive(files);
    HostDataPackStore *store=Store(root,compatibility); [store beginLaunch];

    /* Python's xz (the publisher) decodes with Apple's COMPRESSION_LZMA, and the same archive is reused afterward. */
    if (argc > 1) {
        NSData *python=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
        CHECK(python.length > 0, "publisher archive fixture is readable");
        NSData *decoded=nil; {
            NSMutableData *out=[NSMutableData dataWithLength:raw.length+1];
            size_t n=compression_decode_buffer(out.mutableBytes,out.length,python.bytes,python.length,NULL,COMPRESSION_LZMA);
            out.length=n; decoded=out;
        }
        CHECK([decoded isEqual:raw], "COMPRESSION_LZMA decodes the publisher's xz stream byte for byte");
        Update(store,Index(1,raw,python,0));
        CHECK(BlobRequests()==1, "first index downloads its archive once");
        store=Store(root,compatibility); NSString *folder=[store beginLaunch]; [store markHealthy];
        CHECK([[store metadata][@"sequence"] integerValue]==1, "publisher archive activates on the next launch");
        for (NSString *name in files) CHECK([[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:name]] isEqual:files[name]], "expanded file matches its source");
    }
    NSData *xz=XZ(raw);
    [counts removeAllObjects];
    Update(store,Index(2,raw,xz,0));
    store=Store(root,compatibility); NSString *folder=[store beginLaunch]; [store markHealthy];
    CHECK([[store metadata][@"sequence"] integerValue]==2, "Apple-encoded archive activates");
    for (NSString *name in files) CHECK([[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:name]] isEqual:files[name]], "Apple-encoded files match");
    /* A later signed sequence for the same files needs no archive download: the content address is the cache key. */
    [counts removeAllObjects];
    NSData *three=Index(3,raw,xz,0); [counts removeAllObjects]; Update(store,three);
    CHECK(BlobRequests()==0, "same archive under a new sequence is not downloaded again");
    store=Store(root,compatibility); [store beginLaunch]; [store markHealthy];
    CHECK([[store metadata][@"sequence"] integerValue]==3, "cached archive activates under the new sequence");
    /* A damaged cached file fails the canonical archive hash, so the archive is fetched again. */
    folder=[store beginLaunch];
    [@"corrupt" writeToFile:[folder stringByAppendingPathComponent:@"a.actions"] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    [counts removeAllObjects]; Update(store,Index(4,raw,xz,0));
    CHECK(BlobRequests()==1, "damaged cached file triggers a fresh archive download");
    store=Store(root,compatibility); [store beginLaunch]; [store markHealthy];
    CHECK([[store metadata][@"sequence"] integerValue]==4, "fresh archive replaces the damaged generation");

    /* Everything below must leave sequence 4 healthy. */
    /* A archive not yet held, so the cache cannot answer for it. */
    NSData *fresh=Archive(@{@"a.input":Bytes(@"fresh\n")}), *freshXZ=XZ(fresh);
    NSMutableData *flipped=[freshXZ mutableCopy]; ((unsigned char *)flipped.mutableBytes)[flipped.length/2]^=0x40;
    NSArray *cases=@[
        @[@"served bytes differ from the signed compressed hash", Index(5,fresh,freshXZ,0), SHA(freshXZ), flipped],
        @[@"raw size smaller than the stream (decompression bomb)", Index(6,fresh,freshXZ,10), SHA(freshXZ), freshXZ],
        @[@"signed raw size too large", Index(7,fresh,freshXZ,200000000), SHA(freshXZ), freshXZ],
        @[@"names out of order", Index(8,Archive(@{@"a.input":Bytes(@"x"),@"b.input":Bytes(@"y")}),xz,0), SHA(xz), xz],
    ];
    for (NSArray *item in cases) {
        responses[[[@"/packs/recording-blobs/" stringByAppendingString:item[2]] stringByAppendingString:@".xz"]]=item[3];
        Update(store,item[1]);
        store=Store(root,compatibility); [store beginLaunch];
        CHECK([[store metadata][@"sequence"] integerValue]==4, [item[0] UTF8String]);
    }
    /* Malformed archives that are correctly signed and hashed still fail the strict parser. */
    NSMutableData *unsorted=[Bytes(@"visr-recordings-1\n") mutableCopy];
    [unsorted appendData:Bytes(@"b.input 1\nx\na.input 1\ny\n")];
    NSMutableData *extra=[raw mutableCopy]; [extra appendData:Bytes(@"junk")];
    NSMutableData *badName=[Bytes(@"visr-recordings-1\n") mutableCopy]; [badName appendData:Bytes(@"A.input 1\nx\n")];
    NSMutableData *noHeader=[Bytes(@"nonsense\n") mutableCopy]; [noHeader appendData:Bytes(@"a.input 1\nx\n")];
    NSMutableData *zero=[Bytes(@"visr-recordings-1\n") mutableCopy]; [zero appendData:Bytes(@"a.input 0\n\n")];
    NSArray *malformed=@[unsorted,extra,badName,noHeader,zero,[NSData data]];
    NSInteger index=20;
    for (NSData *bad in malformed) {
        if (!bad.length) continue;
        Update(store,Index(index++,bad,XZ(bad),0));
        store=Store(root,compatibility); [store beginLaunch];
        CHECK([[store metadata][@"sequence"] integerValue]==4, "malformed archive contents cannot activate");
    }
    /* A catalog-1 store ignores catalog-2 envelopes: the compatibility string is part of what is signed and compared. */
    HostDataPackStore *old=Store([root stringByAppendingString:@"-old"],[@"benchmark-catalog-1:" stringByAppendingString:Guest]); [old beginLaunch];
    Update(old,Index(40,raw,xz,0));
    old=Store([root stringByAppendingString:@"-old"],[@"benchmark-catalog-1:" stringByAppendingString:Guest]); [old beginLaunch];
    CHECK([[old metadata][@"bundled"] boolValue], "a catalog-1 reader never takes a catalog-2 feed");
    [NSFileManager.defaultManager removeItemAtPath:root error:NULL];
    method_setImplementation(method,original);
    if (!failures) puts("recording downloads: xz decode, content-addressed reuse, bounds and strict archive parsing pass");
    return failures?1:0;
} }
