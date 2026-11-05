/* SEND LOG (host_log_send.m): the log files are sent in the order they were written, codes avoid look-alike
   characters, and the row has an empty value until pressed. Sending needs the iCloud entitlement, which a probe lacks. */
#import <Foundation/Foundation.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include "../host/host_log_file.c"
#include "../host/host_log_send.m"

void host_logf(int priority, const char *format, ...) {
    (void)priority; (void)format;
}

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

        /* the files go in the order they were written, each under its name */
        NSString *all = [[NSString alloc] initWithData:combined_logs() encoding:NSUTF8StringEncoding];
        NSRange previous = [all rangeOfString:@"===== ios-runtime.previous.log"];
        NSRange middle = [all rangeOfString:@"===== ios-runtime.rotated.log"];
        NSRange last = [all rangeOfString:@"===== ios-runtime.log"];
        assert(previous.location == 0 && middle.location > previous.location && last.location > middle.location);
        assert([all containsString:@"last session"] && [all hasSuffix:@"the last line\n"]);
        assert([NSString stringWithUTF8String:"SENT. CODE: "].length && new_code().length == 8);
        for (int i = 0; i < 50; i++) assert([new_code() rangeOfCharacterFromSet:
            [NSCharacterSet characterSetWithCharactersInString:@"01IOilo"]].location == NSNotFound);

        /* before a press the row's value is empty (pressing it needs the iCloud entitlement, which a probe lacks) */
        char text[96];
        host_log_send_status(text, sizeof(text));
        assert(text[0] == 0 && host_log_send_available());
        [NSFileManager.defaultManager removeItemAtPath:folder error:nil];
    }
    puts("ok");
    return 0;
}
