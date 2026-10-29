/* Apache-2.0. Discord trial, behind HALO_DISCORD. */
#import <Foundation/Foundation.h>
#if HALO_NATIVE_MAC
#import <AppKit/AppKit.h>
#endif
#import <Security/Security.h>
#ifdef __cplusplus
extern "C" {
#endif
void host_discord_install(void);
#if HALO_NATIVE_MAC
NSMenuItem *host_discord_settings_item(void);
#endif
void host_discord_show_settings(void);
void host_discord_shutdown(void);
/* Tokens never enter logs or defaults. */
OSStatus host_discord_tokens_write(NSString *access, NSString *refresh);
NSDictionary *host_discord_tokens_read(OSStatus *status);
OSStatus host_discord_tokens_delete(void);
#ifdef __cplusplus
}
#endif
