/* Apache-2.0. SEND LOG: one GameLog record in the public database of the report container, holding the
   session's logs. It is sent only when the player presses the About screen's row, and its record name is a short
   code (the benchmark reports' alphabet) the player gives the developer, who looks the record up by it. */
#import <Foundation/Foundation.h>
#include <string.h>
#include <sys/sysctl.h>
#include <zlib.h>
#include "host_log_send.h"
#include "host.h"
#if HALO_PERF_REPORTS
#import <CloudKit/CloudKit.h>
#endif

#define LOG_RECORD_TYPE @"GameLog"
#define SEND_SECONDS 60
#define LOG_FILES @[@"ios-runtime.previous.log", @"ios-runtime.rotated.log", @"ios-runtime.log"]

static char status_text[96];

__attribute__((unused)) static void set_status(const char *text) {
    @synchronized(NSProcessInfo.processInfo) {
        snprintf(status_text, sizeof(status_text), "%s", text);
    }
}

void host_log_send_status(char *buffer, unsigned int size) {
    if (!size) return;
    @synchronized(NSProcessInfo.processInfo) {
        snprintf(buffer, size, "%s", status_text);
    }
}

#if HALO_PERF_REPORTS
static BOOL sending;
void host_log_send_start(void);
int host_log_send_available(void) {
#ifdef HALO_DEVELOPMENT_HOOKS
    /* HALO_SEND_LOG_AUTO=1: sends once, 20 s after launch, as if the row were pressed (for testing the upload
       where nobody can press it). Compiled out of App Store packages. */
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        const char *automatic = getenv("HALO_SEND_LOG_AUTO");
        if (automatic && *automatic == '1')
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC), dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
                ^{ host_log_send_start(); });
    });
#endif
    return 1;
}

static NSData *gzipped(NSData *data) {
    NSMutableData *out = [NSMutableData dataWithLength:compressBound((uLong)data.length) + 32];
    z_stream stream = { 0 };

    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return nil;
    stream.next_in = (Bytef *)data.bytes; stream.avail_in = (uInt)data.length;
    stream.next_out = out.mutableBytes; stream.avail_out = (uInt)out.length;
    if (deflate(&stream, Z_FINISH) != Z_STREAM_END) { deflateEnd(&stream); return nil; }
    out.length = stream.total_out;
    deflateEnd(&stream);
    return out;
}

/* every log file the game keeps, in the order they were written, each under a line naming it */
static NSData *combined_logs(void) {
    NSMutableData *all = [NSMutableData data];
    for (NSString *name in LOG_FILES) {
        NSData *data = [NSData dataWithContentsOfFile:name];
        if (!data.length) continue;
        [all appendData:[[NSString stringWithFormat:@"===== %@ (%lu bytes) =====\n", name,
            (unsigned long)data.length] dataUsingEncoding:NSUTF8StringEncoding]];
        [all appendData:data];
        if (((const char *)data.bytes)[data.length - 1] != '\n') [all appendBytes:"\n" length:1];
    }
    return all;
}

static NSString *new_code(void) {
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   /* nothing that reads as another */
    char code[9];
    for (int i = 0; i < 8; i++) code[i] = alphabet[arc4random_uniform(32)];
    code[8] = 0;
    return @(code);
}

/* the app's build description, as the menus show it (VISRBuildDescription, else the marketing version) */
static NSString *app_version(void) {
    id version = [NSBundle.mainBundle objectForInfoDictionaryKey:@"VISRBuildDescription"];
    if (![version isKindOfClass:NSString.class] || ![version length])
        version = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    return [version isKindOfClass:NSString.class] ? version : @"unknown";
}

static NSString *machine(void) {
    char model[128] = "unknown";
    size_t length = sizeof(model);
#if HALO_NATIVE_MAC || TARGET_OS_MACCATALYST
    sysctlbyname("hw.model", model, &length, NULL, 0);
#else
    sysctlbyname("hw.machine", model, &length, NULL, 0);
#endif
    return @(model);
}

static NSString *platform_name(void) {
#if HALO_NATIVE_MAC
    return @"macos";
#elif TARGET_OS_MACCATALYST
    return @"maccatalyst";
#elif TARGET_OS_VISION
    return @"visionos";
#elif TARGET_OS_TV
    return @"tvos";
#else
    return @"ios";
#endif
}

/* returns the sentence for the row; success when it begins "SENT" */
static NSString *upload(NSString *code) {
    NSData *logs = combined_logs();
    if (!logs.length) return @"NO LOG TO SEND";
    NSData *packed = gzipped(logs);
    NSURL *file = [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:
        [NSString stringWithFormat:@"visr-log-%@.txt.gz", code]];
    if (!packed || ![packed writeToURL:file atomically:YES]) return @"NOT SENT";

    CKContainer *container = [CKContainer containerWithIdentifier:@HALO_PERF_CLOUD];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block CKAccountStatus account = CKAccountStatusCouldNotDetermine;
    __block NSError *failure = nil;
    [container accountStatusWithCompletionHandler:^(CKAccountStatus status, NSError *error) {
        (void)error;
        account = status;
        dispatch_semaphore_signal(done);
    }];
    if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC)) != 0) {
        [NSFileManager.defaultManager removeItemAtURL:file error:nil];
        return @"NOT SENT. NO CONNECTION";
    }
    if (account != CKAccountStatusAvailable) {
        host_logf(HOST_LOG_INFO, "send log: not sent: iCloud account status %ld", (long)account);
        [NSFileManager.defaultManager removeItemAtURL:file error:nil];
        return @"NOT SENT. SIGN IN TO ICLOUD";
    }
    NSDateFormatter *stamp = [NSDateFormatter new];
    stamp.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    CKRecord *record = [[CKRecord alloc] initWithRecordType:LOG_RECORD_TYPE recordID:
        [[CKRecordID alloc] initWithRecordName:[@"log-" stringByAppendingString:code]]];
    record[@"code"] = code;
    record[@"model"] = machine();
    record[@"platform"] = platform_name();
    record[@"osVersion"] = NSProcessInfo.processInfo.operatingSystemVersionString;
    record[@"appVersion"] = app_version();
    record[@"appBuild"] = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleVersion"] ?: @"unknown";
    record[@"sent"] = [NSDate date];
    record[@"bytes"] = @(logs.length);
    record[@"log"] = [[CKAsset alloc] initWithFileURL:file];
    CKModifyRecordsOperation *operation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:@[record] recordIDsToDelete:nil];
    operation.savePolicy = CKRecordSaveAllKeys;
    operation.atomic = YES;
    operation.configuration.timeoutIntervalForRequest = SEND_SECONDS;
    operation.configuration.timeoutIntervalForResource = SEND_SECONDS;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    operation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *saved, NSArray<CKRecordID *> *deleted, NSError *error) {
        (void)saved; (void)deleted;
        failure = error;
        dispatch_semaphore_signal(done);
    };
#pragma clang diagnostic pop
    [container.publicCloudDatabase addOperation:operation];
    if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, (SEND_SECONDS + 5) * NSEC_PER_SEC)) != 0) {
        [operation cancel];
        [NSFileManager.defaultManager removeItemAtURL:file error:nil];
        host_logf(HOST_LOG_INFO, "send log: not sent: timed out");
        return @"NOT SENT. TRY AGAIN";
    }
    [NSFileManager.defaultManager removeItemAtURL:file error:nil];
    if (failure) {
        host_logf(HOST_LOG_INFO, "send log: not sent: %s (%s %ld)", failure.localizedDescription.UTF8String,
            failure.domain.UTF8String, (long)failure.code);
        BOOL offline = [failure.domain isEqual:CKErrorDomain] && (failure.code == CKErrorNetworkUnavailable ||
            failure.code == CKErrorNetworkFailure || failure.code == CKErrorServiceUnavailable);
        return offline ? @"NOT SENT. NO CONNECTION" : @"NOT SENT. TRY AGAIN";
    }
    host_logf(HOST_LOG_INFO, "send log: sent as %s (%lu bytes of log)", code.UTF8String, (unsigned long)logs.length);
    return [NSString stringWithFormat:@"SENT. CODE: %@", code];
}

void host_log_send_start(void) {
    @synchronized(NSProcessInfo.processInfo) {
        if (sending) return;
        sending = YES;
    }
    set_status("SENDING...");
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        @autoreleasepool {
            NSString *result = nil;
            @try {
                result = upload(new_code());
            } @catch (NSException *exception) {
                host_logf(HOST_LOG_INFO, "send log: not sent: %s", exception.reason.UTF8String);
                result = @"NOT SENT. TRY AGAIN";
            }
            set_status(result.UTF8String);
            @synchronized(NSProcessInfo.processInfo) { sending = NO; }
        }
    });
}
#elif defined(HALO_DEVELOPMENT_HOOKS)
/* A development build without the iCloud entitlement can show the row and its answers for the Mac runner's pictures:
   HALO_SEND_LOG_PREVIEW=ready (the row, blank), or sent, icloud or offline (the row as it reads after a press that
   ended so; pressing it again shows the same). Nothing is uploaded. Compiled out of App Store packages. */
static const char *preview_text(const char *kind) {
    if (!strcmp(kind, "sent")) return "SENT. CODE: ABCD1234";
    if (!strcmp(kind, "offline")) return "NOT SENT. NO CONNECTION";
    if (!strcmp(kind, "icloud")) return "NOT SENT. SIGN IN TO ICLOUD";
    return "";
}
int host_log_send_available(void) {
    const char *kind = getenv("HALO_SEND_LOG_PREVIEW");
    if (!kind || !*kind) return 0;
    set_status(preview_text(kind));
    return 1;
}
void host_log_send_start(void) {
    const char *kind = getenv("HALO_SEND_LOG_PREVIEW");
    set_status(kind && strcmp(kind, "ready") ? preview_text(kind) : "NOT SENT. SIGN IN TO ICLOUD");
}
#else
int host_log_send_available(void) { return 0; }
void host_log_send_start(void) {}
#endif
