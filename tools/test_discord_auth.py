"""Exercise OAuth parsing and PKCE without the proprietary SDK or a login."""
from pathlib import Path
import subprocess
import sys
import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(sys.platform != 'darwin', reason='Foundation requires macOS')
def test_pkce_and_callback_validation(tmp_path):
    probe = tmp_path / 'auth.m'
    probe.write_text(r'''
#import "host_discord_auth.h"
#include <assert.h>
int main(void) { @autoreleasepool {
    NSString *verifier = @"dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    assert([host_discord_pkce_challenge(verifier) isEqualToString:@"E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"]);
    assert(!host_discord_pkce_challenge(@"too-short"));
    NSString *state = host_discord_random_state();
    assert(state.length == 43 && ![state isEqualToString:host_discord_random_state()]);
    NSURLComponents *auth = [NSURLComponents componentsWithURL:host_discord_authorization_url(verifier, state) resolvingAgainstBaseURL:NO];
    assert([auth.host isEqualToString:@"discord.com"]);
    NSMutableDictionary *query = [NSMutableDictionary new];
    for (NSURLQueryItem *item in auth.queryItems) query[item.name] = item.value;
    assert([query[@"redirect_uri"] isEqualToString:VISRDiscordRedirect]);
    assert([query[@"code_challenge_method"] isEqualToString:@"S256"]);
    assert([query[@"scope"] isEqualToString:@"openid sdk.social_layer_presence"]);
    assert(!query[@"client_secret"]);
    NSURLComponents *redacted = [NSURLComponents componentsWithString:host_discord_redacted_authorization_url(auth.URL)];
    NSMutableDictionary *safe = [NSMutableDictionary new];
    for (NSURLQueryItem *item in redacted.queryItems) safe[item.name] = item.value;
    assert([safe[@"state"] isEqualToString:@"REDACTED"]);
    assert([safe[@"code_challenge"] isEqualToString:@"REDACTED"]);
    assert([safe[@"client_id"] isEqualToString:query[@"client_id"]]);
    assert([safe[@"redirect_uri"] isEqualToString:VISRDiscordRedirect]);
    assert(![redacted.string containsString:state]);
    assert(![redacted.string containsString:query[@"code_challenge"]]);
    assert(!host_discord_callback_code(nil, @"expected"));
    NSString *base = [VISRDiscordRedirect stringByAppendingString:@"?state=expected&code=a%2Bb%3D&iss=https%3A%2F%2Fdiscord.com"];
    assert([host_discord_callback_code([NSURL URLWithString:base], @"expected") isEqualToString:@"a+b="]);
    assert(!host_discord_callback_code([NSURL URLWithString:base], @"wrong"));
    assert(!host_discord_callback_code([NSURL URLWithString:base], @""));
    for (NSString *suffix in @[@"?code=x", @"?state=expected&code=", @"?state=expected&code=x&code=y",
        @"?state=expected&state=expected&code=x", @"?state=expected&code=x&error=access_denied",
        @"?state=expected&code=x&iss=https://evil.example", @"?state=expected&code=x#fragment"]) {
        assert(!host_discord_callback_code([NSURL URLWithString:[VISRDiscordRedirect stringByAppendingString:suffix]], @"expected"));
    }
    for (NSString *base in @[@"https://discord.com/authorize/callback", @"discord-1565271043529117697://evil/authorize/callback",
        @"discord-1565271043529117697:/authorize/other", @"discord-1:/authorize/callback"]) {
        assert(!host_discord_callback_code([NSURL URLWithString:[base stringByAppendingString:@"?state=expected&code=x"]], @"expected"));
    }
} return 0; }
''')
    executable = tmp_path / 'auth'
    subprocess.run(['xcrun', 'clang', '-fobjc-arc', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT / 'port/ios/host'), str(probe),
                    str(ROOT / 'port/ios/host/host_discord_auth.m'), '-framework', 'Foundation',
                    '-framework', 'Security', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
