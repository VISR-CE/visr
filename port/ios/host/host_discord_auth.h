/* Apache-2.0. OAuth values shared by the native Mac and iOS trial. */
#import <Foundation/Foundation.h>
#ifdef __cplusplus
extern "C" {
#endif
extern NSString *const VISRDiscordRedirect;
NSString *host_discord_random_state(void);
NSString *host_discord_pkce_challenge(NSString *verifier);
NSURL *host_discord_authorization_url(NSString *verifier, NSString *state);
/* Suitable for development logs; state and PKCE challenge are removed. */
NSString *host_discord_redacted_authorization_url(NSURL *url);
/* Invalid, canceled and unsolicited callbacks return nil. Never log the URL. */
NSString *host_discord_callback_code(NSURL *url, NSString *expected_state);
#ifdef __cplusplus
}
#endif
