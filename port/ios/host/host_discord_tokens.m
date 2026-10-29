/* Apache-2.0. Local-only Discord tokens; never store them in defaults or logs. */
#import "host_discord.h"

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
