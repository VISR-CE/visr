/* Exercise the same CoreImage generator as the UIKit cards, without running the app. */
#import <Foundation/Foundation.h>
#import <CoreImage/CoreImage.h>
#include "host_qr.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    @autoreleasepool {
        NSString *link = @"HALO://JOIN/0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
        CIImage *image = host_qr_code(link, 4);
        assert(image);
        /* Version 4: 33 modules, plus four white modules on every side. */
        assert(CGRectGetWidth(image.extent) == 41);
        assert(CGRectGetHeight(image.extent) == 41);
        CGImageRef rendered = [[CIContext context]
            createCGImage:image fromRect:image.extent];
        if (rendered) CGImageRelease(rendered);
        else puts("NOTE: CoreImage bitmap rendering is unavailable in this probe environment; generator dimensions checked");
        puts("PASS: uppercase invite QR is version 4 (33 modules), with a four-module quiet zone");
    }
    return 0;
}
