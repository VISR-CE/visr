/* Apache-2.0. host_open_website.h */
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#include "host_open_website.h"
#if HALO_NATIVE_MAC
#import <AppKit/AppKit.h>
#elif !TARGET_OS_TV
#import <UIKit/UIKit.h>
#endif

#define WEBSITE_URL @"https://visr.gg/"

void host_open_website(void) {
#if TARGET_OS_TV
    return;
#else
    dispatch_async(dispatch_get_main_queue(), ^{
        NSURL *url = [NSURL URLWithString:WEBSITE_URL];
#if HALO_NATIVE_MAC
        [NSWorkspace.sharedWorkspace openURL:url];
#else
        [UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];
#endif
    });
#endif
}
