/* The host's log file (host_log_file.c): the previous session's log is kept, and a log past its limit rotates
   over the same descriptor (host_crash.c holds it). */
#import <Foundation/Foundation.h>
#include <assert.h>
#include <unistd.h>
#include "../host/host_log_file.c"


static NSString *read_file(NSString *name) {
    return [NSString stringWithContentsOfFile:name encoding:NSUTF8StringEncoding error:nil] ?: @"";
}

int main(void) {
    @autoreleasepool {
        NSString *folder = [NSTemporaryDirectory() stringByAppendingPathComponent:
            [NSString stringWithFormat:@"log-send-probe-%d", getpid()]];
        [NSFileManager.defaultManager createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil];
        assert(chdir(folder.fileSystemRepresentation) == 0);

        /* the session before: kept as previous; an older previous and a rotated file go */
        [@"older\n" writeToFile:@"ios-runtime.previous.log" atomically:YES encoding:NSUTF8StringEncoding error:nil];
        [@"rotated of the last\n" writeToFile:@"ios-runtime.rotated.log" atomically:YES encoding:NSUTF8StringEncoding error:nil];
        [@"last session\n" writeToFile:@"ios-runtime.log" atomically:YES encoding:NSUTF8StringEncoding error:nil];
        FILE *log = host_log_file_open(300);
        assert(log);
        int descriptor = fileno(log);
        assert([read_file(@"ios-runtime.previous.log") isEqual:@"last session\n"]);
        assert(access("ios-runtime.rotated.log", F_OK) != 0);
        assert([read_file(@"ios-runtime.log") length] == 0);

        /* an empty log does not replace the previous one at the next start */
        host_log_file_line("first");
        for (int i = 0; i < 40; i++) host_log_file_line("a line of the current session, number x");
        host_log_file_line("the last line");
        assert(fileno(log) == descriptor);
        NSString *rotated = read_file(@"ios-runtime.rotated.log"), *current = read_file(@"ios-runtime.log");
        assert(rotated.length > 0 && rotated.length <= 340); /* (the last of several rotations) */
        assert([current hasPrefix:@"(log continued"] && [current hasSuffix:@"the last line\n"]);
        assert(current.length <= 340);

        [NSFileManager.defaultManager removeItemAtPath:folder error:nil];
    }
    puts("ok");
    return 0;
}
