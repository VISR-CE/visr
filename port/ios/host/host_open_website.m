/* Apache-2.0. host_open_website.h */
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#include "host_open_website.h"
#if HALO_NATIVE_MAC
#import <AppKit/AppKit.h>
#elif !TARGET_OS_TV
#import <UIKit/UIKit.h>
#endif

static NSString *const kPages[] = {
    @"https://visr.gg/",
    @"https://visr.gg/privacy",
    @"https://github.com/steverice/visr/blob/main/LICENSE",
};

void host_open_website(int page) {
#if TARGET_OS_TV
    (void)page;
    return;
#else
    if (page < 0 || page >= (int)(sizeof(kPages) / sizeof(kPages[0]))) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSURL *url = [NSURL URLWithString:kPages[page]];
#if HALO_NATIVE_MAC
        [NSWorkspace.sharedWorkspace openURL:url];
#else
        [UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];
#endif
    });
#endif
}
