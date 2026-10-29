/* Apache-2.0. One display identity for the menu, logs and reports; updates use the numeric bundle version. */
#ifndef HOST_APP_VERSION_H
#define HOST_APP_VERSION_H

#include <CoreFoundation/CoreFoundation.h>

static inline CFStringRef host_app_version_string(CFBundleRef bundle)
{
    CFTypeRef version = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("VISRBuildDescription"));
    if (!version || CFGetTypeID(version) != CFStringGetTypeID() || !CFStringGetLength((CFStringRef)version))
        version = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString"));
    return version && CFGetTypeID(version) == CFStringGetTypeID() ? (CFStringRef)version : CFSTR("unknown");
}

#ifdef __OBJC__
#import <Foundation/Foundation.h>
static inline NSString *host_app_version_description(void)
{
    return (__bridge NSString *)host_app_version_string(CFBundleGetMainBundle());
}
#endif

#endif
