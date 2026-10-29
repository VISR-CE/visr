/* Apache-2.0. Preflight only: SDK calls wait for the portal archive. */
#import <Security/Security.h>
#import "host_discord.h"
#include <SDL3/SDL.h>
extern "C" {
#include "ios_host.h"
}

static NSDictionary *token_query(void) {
    return @{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: [NSBundle.mainBundle.bundleIdentifier stringByAppendingString:@".discord"],
        (__bridge id)kSecAttrAccount: @"1565271043529117697",
        (__bridge id)kSecAttrSynchronizable: @NO};
}
OSStatus host_discord_tokens_write(NSString *access, NSString *refresh) {
    if (!access.length || !refresh.length) return errSecParam;
    NSData *data = [NSJSONSerialization dataWithJSONObject:@{@"access":access, @"refresh":refresh} options:0 error:nil];
    if (!data) return errSecParam;
    NSDictionary *attributes = @{(__bridge id)kSecValueData:data,
        (__bridge id)kSecAttrAccessible:(__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly};
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)token_query(), (__bridge CFDictionaryRef)attributes);
    if (status == errSecItemNotFound) {
        NSMutableDictionary *item = [token_query() mutableCopy];
        [item addEntriesFromDictionary:attributes];
        status = SecItemAdd((__bridge CFDictionaryRef)item, NULL);
    }
    return status;
}
NSDictionary *host_discord_tokens_read(OSStatus *status) {
    NSMutableDictionary *query = [token_query() mutableCopy];
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = NULL;
    OSStatus value = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status) *status = value;
    if (value != errSecSuccess) return nil;
    id tokens = [NSJSONSerialization JSONObjectWithData:CFBridgingRelease(result) options:0 error:nil];
    if (![tokens isKindOfClass:NSDictionary.class] ||
        ![tokens[@"access"] isKindOfClass:NSString.class] || ![tokens[@"access"] length] ||
        ![tokens[@"refresh"] isKindOfClass:NSString.class] || ![tokens[@"refresh"] length]) {
        if (status) *status = errSecDecode;
        return nil;
    }
    return tokens;
}
OSStatus host_discord_tokens_delete(void) {
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)token_query());
    return status == errSecItemNotFound ? errSecSuccess : status;
}

static bool SDLCALL discord_url(void *context, SDL_Event *event) {
    (void)context;
    if (event->type != SDL_EVENT_DROP_FILE || !event->drop.data) return true;
    NSURLComponents *url = [NSURLComponents componentsWithString:@(event->drop.data)];
    if ([url.scheme.lowercaseString isEqualToString:@"discord-1565271043529117697"] &&
        !url.host.length && [url.path isEqualToString:@"/authorize/callback"]) {
        /* Never log the URL: it can contain an authorization code. A real SDK
         * callback must first match the pending PKCE request's state. */
        host_logf(HOST_LOG_INFO, "Discord: callback received; ignored because SDK authorization is unavailable");
    }
    return true;
}
void host_discord_install(void) {
    if (!SDL_AddEventWatch(discord_url, NULL))
        host_logf(HOST_LOG_ERROR, "Discord: could not watch URL callbacks");
    host_logf(HOST_LOG_INFO, "Discord: preflight only; SDK not linked");
}

@interface VISRDiscordSettings : NSObject
- (void)show:(id)sender;
@end
@implementation VISRDiscordSettings
- (void)show:(id)sender {
    (void)sender;
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Discord";
    alert.informativeText = @"Not linked. This trial needs the Discord Social SDK before account linking is available. Linking will share your map, game mode, and player counts with Discord.";
    NSButton *link = [alert addButtonWithTitle:@"Link Discord"];
    link.enabled = NO;
    [alert addButtonWithTitle:@"Done"];
    [alert runModal];
}
@end
NSMenuItem *host_discord_settings_item(void) {
    static VISRDiscordSettings *settings;
    if (!settings) settings = [VISRDiscordSettings new];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Discord Settings…" action:@selector(show:) keyEquivalent:@""];
    item.target = settings;
    return item;
}
