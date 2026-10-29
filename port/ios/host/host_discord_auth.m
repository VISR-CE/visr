/* Apache-2.0. The SDK desktop Authorize helper uses a loopback server. Building
 * the standard OAuth request here lets both hosts use our registered scheme. */
#import "host_discord_auth.h"
#import <CommonCrypto/CommonDigest.h>
#import <Security/Security.h>
NSString *const VISRDiscordRedirect = @"discord-1565271043529117697:/authorize/callback";
static NSString *base64url(NSData *bytes) {
    NSString *text = [bytes base64EncodedStringWithOptions:0];
    return [[[text stringByReplacingOccurrencesOfString:@"+" withString:@"-"]
        stringByReplacingOccurrencesOfString:@"/" withString:@"_"] stringByReplacingOccurrencesOfString:@"=" withString:@""];
}
NSString *host_discord_random_state(void) {
    unsigned char bytes[32];
    if (SecRandomCopyBytes(kSecRandomDefault, sizeof(bytes), bytes) != errSecSuccess) return nil;
    return base64url([NSData dataWithBytes:bytes length:sizeof(bytes)]);
}
NSString *host_discord_pkce_challenge(NSString *verifier) {
    NSCharacterSet *allowed = [NSCharacterSet characterSetWithCharactersInString:
        @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"];
    if (verifier.length < 43 || verifier.length > 128 ||
        [verifier rangeOfCharacterFromSet:allowed.invertedSet].location != NSNotFound) return nil;
    NSData *data = [verifier dataUsingEncoding:NSASCIIStringEncoding];
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);
    return base64url([NSData dataWithBytes:digest length:sizeof(digest)]);
}
NSURL *host_discord_authorization_url(NSString *verifier, NSString *state) {
    NSString *challenge = host_discord_pkce_challenge(verifier);
    if (!challenge || !state.length) return nil;
    NSURLComponents *url = [NSURLComponents componentsWithString:@"https://discord.com/oauth2/authorize"];
    NSMutableArray *items = [NSMutableArray new];
    NSDictionary *values = @{@"client_id": @"1565271043529117697", @"response_type": @"code",
        @"redirect_uri": VISRDiscordRedirect, @"scope": @"openid sdk.social_layer_presence",
        @"state": state, @"code_challenge": challenge, @"code_challenge_method": @"S256", @"prompt": @"consent"};
    for (NSString *key in values) [items addObject:[NSURLQueryItem queryItemWithName:key value:values[key]]];
    url.queryItems = items;
    return url.URL;
}
NSString *host_discord_redacted_authorization_url(NSURL *url) {
    NSURLComponents *safe = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    NSMutableArray *items = [NSMutableArray new];
    for (NSURLQueryItem *item in safe.queryItems) {
        BOOL sensitive = [item.name isEqualToString:@"state"] || [item.name isEqualToString:@"code_challenge"];
        [items addObject:[NSURLQueryItem queryItemWithName:item.name value:sensitive ? @"REDACTED" : item.value]];
    }
    safe.queryItems = items;
    return safe.string;
}
NSString *host_discord_callback_code(NSURL *url, NSString *expected_state) {
    if (!url) return nil;
    NSURLComponents *value = [NSURLComponents componentsWithURL:url resolvingAgainstBaseURL:NO];
    if (!expected_state.length || ![value.scheme.lowercaseString isEqualToString:@"discord-1565271043529117697"] ||
        value.host.length || value.user || value.password || value.port || value.fragment ||
        ![value.path isEqualToString:@"/authorize/callback"]) return nil;
    NSMutableDictionary *query = [NSMutableDictionary new];
    for (NSURLQueryItem *item in value.queryItems) {
        if (query[item.name] || !item.value) return nil;
        query[item.name] = item.value;
    }
    if (![query[@"state"] isEqualToString:expected_state] || query[@"error"] ||
        (query[@"iss"] && ![query[@"iss"] isEqualToString:@"https://discord.com"])) return nil;
    return [query[@"code"] length] ? query[@"code"] : nil;
}
