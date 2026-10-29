/* Apache-2.0. Native Mac trial only, behind HALO_DISCORD. */
#import <AppKit/AppKit.h>
#import <Security/Security.h>
#ifdef __cplusplus
extern "C" {
#endif
void host_discord_install(void);
NSMenuItem *host_discord_settings_item(void);
/* SDK callbacks will use these helpers; tokens never enter logs or defaults. */
OSStatus host_discord_tokens_write(NSString *access, NSString *refresh);
NSDictionary *host_discord_tokens_read(OSStatus *status);
OSStatus host_discord_tokens_delete(void);
#ifdef __cplusplus
}
#endif
