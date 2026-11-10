#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#if HALO_NATIVE_MAC
#include "host_macos.h"
#elif !TARGET_OS_OSX
#import <UIKit/UIKit.h>
#endif
#include "host_startup.h"
#include <stdio.h>
#include <string.h>

void host_startup_read_live(struct host_startup_info *info) {
    @autoreleasepool {
        memset(info, 0, sizeof(*info));
        snprintf(info->documents_root, sizeof(info->documents_root), "%s",
            NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject.fileSystemRepresentation ?: "");
        snprintf(info->caches_root, sizeof(info->caches_root), "%s",
            NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES).firstObject.fileSystemRepresentation ?: "");
        snprintf(info->application_support_root, sizeof(info->application_support_root), "%s",
            NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject.fileSystemRepresentation ?: "");
#if HALO_NATIVE_MAC
        int width = 0, height = 0;
        host_macos_screen_size(&width, &height);
        info->pixel_width = width;
        info->pixel_height = height;
        info->maximum_frames_per_second = host_macos_refresh_rate();
#elif !TARGET_OS_OSX
        snprintf(info->device_model, sizeof(info->device_model), "%s", UIDevice.currentDevice.model.UTF8String ?: "");
        info->phone = UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPhone;
#if !TARGET_OS_VISION
        UIScreen *screen = UIScreen.mainScreen;
        info->screen_scale = screen.scale;
        info->pixel_width = screen.nativeBounds.size.width;
        info->pixel_height = screen.nativeBounds.size.height;
        info->maximum_frames_per_second = screen.maximumFramesPerSecond;
#endif
#endif
    }
}
