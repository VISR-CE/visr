/* CoreImage's unscaled code and a UIKit image with whole-pixel modules.
   quiet_zone is the number of white modules on each edge (four for invites;
   one preserves the existing benchmark and TV import images). */
#pragma once
#import <CoreImage/CoreImage.h>
#include <TargetConditionals.h>
CIImage *host_qr_code(NSString *text, NSUInteger quiet_zone);
#if !TARGET_OS_OSX
#import <UIKit/UIKit.h>
UIImage *host_qr_image(NSString *text, NSUInteger scale, NSUInteger quiet_zone);
#endif
