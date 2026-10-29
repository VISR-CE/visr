/* Apache-2.0. Optional Social SDK trial; all SDK operations run on the main thread. */
#import "host_discord.h"
#import "host_discord_auth.h"
#import <AuthenticationServices/AuthenticationServices.h>
#if !HALO_NATIVE_MAC
#import <UIKit/UIKit.h>
#endif
#include <SDL3/SDL.h>
#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
extern "C" {
#include "ios_host.h"
#include "host_discord_presence.h"
#if !HALO_NATIVE_MAC
void host_discord_ui_install(void);
#endif
}
#include <memory>
#include <string>
#include <cstring>

static constexpr uint64_t application_id = 1565271043529117697ULL;
static std::unique_ptr<discordpp::Client> client;
static NSString *pending_state, *verifier, *status_text = @"Not linked";
static NSTimeInterval authorization_started;
static unsigned generation;
static bool linked, refreshing, presence_pending;
static NSTimer *callback_timer;
static struct discord_presence published;
static bool published_valid;
static unsigned refresh_attempts;
static NSTimeInterval refresh_retry;
static ASWebAuthenticationSession *authentication;

static void report_result(const char *step, const discordpp::ClientResult &result) {
    /* Response bodies and SDK log messages can contain credentials. */
    host_logf(HOST_LOG_INFO, "Discord: %s %s (type %d, code %d)", step,
        result.Successful() ? "succeeded" : "failed", (int)result.Type(), result.ErrorCode());
}
static void publish_presence(void) {
    if (!client || !linked || presence_pending || client->GetStatus() != discordpp::Client::Status::Ready) return;
    struct discord_presence snapshot;
    host_discord_presence_snapshot(&snapshot);
    if (published_valid && !memcmp(&snapshot, &published, sizeof(snapshot))) return;
    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Playing);
    activity.SetName(snapshot.name);
    activity.SetDetails(snapshot.details);
    if (snapshot.state[0]) activity.SetState(snapshot.state);
    discordpp::ActivityAssets assets;
    assets.SetLargeImage(snapshot.image);
    assets.SetLargeText("VISR: CE");
    activity.SetAssets(assets);
    if (snapshot.capacity > 0 && snapshot.players > 0) {
        discordpp::ActivityParty party;
        party.SetCurrentSize(snapshot.players);
        party.SetMaxSize(snapshot.capacity);
        activity.SetParty(party);
    }
    if (snapshot.started > 0) {
        discordpp::ActivityTimestamps timestamps;
        timestamps.SetStart(snapshot.started);
        activity.SetTimestamps(timestamps);
    }
    unsigned request = generation;
    presence_pending = true;
    client->UpdateRichPresence(activity, [request, snapshot](discordpp::ClientResult result) {
        if (request != generation) return;
        presence_pending = false;
        report_result("presence", result);
        if (result.Successful()) { published = snapshot; published_valid = true; }
    });
}
static void refresh_tokens(void);
static discordpp::Client::TokenExchangeCallback token_callback(unsigned request, bool is_refresh = false) {
    return [request, is_refresh](discordpp::ClientResult result, std::string access, std::string refresh,
        discordpp::AuthorizationTokenType type, int32_t expires, std::string scopes) {
        (void)expires; (void)scopes;
        if (request != generation || !client) return;
        refreshing = false;
        report_result("token exchange", result);
        if (!result.Successful()) {
            if (is_refresh && result.Retryable() && refresh_attempts < 4) {
                refresh_retry = NSProcessInfo.processInfo.systemUptime + (5u << refresh_attempts++);
                status_text = @"Discord is unavailable. Retrying the saved link…";
            } else {
                linked = false;
                client->Disconnect();
                status_text = @"Could not link. Try Link Discord again.";
            }
            return;
        }
        refresh_retry = 0; refresh_attempts = 0;
        OSStatus saved = host_discord_tokens_write(@(access.c_str()), @(refresh.c_str()));
        if (saved != errSecSuccess) {
            host_logf(HOST_LOG_ERROR, "Discord: Keychain write failed (%d)", (int)saved);
            linked = false;
            client->Disconnect();
            status_text = @"Could not save the Discord link in Keychain.";
            return;
        }
        linked = true;
        status_text = @"Connecting…";
        host_logf(HOST_LOG_INFO, "Discord: tokens saved in Keychain");
        client->UpdateToken(type, access, [request](discordpp::ClientResult updated) {
            if (request != generation || !client) return;
            report_result("UpdateToken", updated);
            if (updated.Successful()) {
                host_logf(HOST_LOG_INFO, "Discord: Connect requested");
                client->Connect();
            } else { linked = false; status_text = @"Could not connect. Try linking again."; }
        });
    };
}
static void refresh_tokens(void) {
    if (!client || refreshing) return;
    OSStatus saved;
    NSDictionary *tokens = host_discord_tokens_read(&saved);
    if (!tokens) {
        if (saved != errSecItemNotFound) host_logf(HOST_LOG_ERROR, "Discord: Keychain read failed (%d)", (int)saved);
        status_text = @"Not linked";
        return;
    }
    refreshing = true;
    linked = true;
    status_text = @"Refreshing Discord link…";
    host_logf(HOST_LOG_INFO, "Discord: refreshing saved token");
    client->RefreshToken(application_id, [tokens[@"refresh"] UTF8String], token_callback(generation, true));
}
static void receive_callback(NSURL *url) {
    if (!client || !pending_state.length || NSProcessInfo.processInfo.systemUptime - authorization_started > 300) return;
    NSString *code = host_discord_callback_code(url, pending_state);
    if (!code) {
        host_logf(HOST_LOG_INFO, "Discord: callback rejected (invalid, mismatched, or canceled)");
        return;
    }
    NSString *request_verifier = verifier;
    pending_state = nil; verifier = nil;
    [authentication cancel]; authentication = nil;
    status_text = @"Completing Discord link…";
    host_logf(HOST_LOG_INFO, "Discord: matching callback received; GetToken requested");
    client->GetToken(application_id, code.UTF8String, request_verifier.UTF8String,
        VISRDiscordRedirect.UTF8String, token_callback(generation));
}
static bool SDLCALL discord_url(void *context, SDL_Event *event) {
    (void)context;
    if (event->type == SDL_EVENT_DROP_FILE && event->drop.data) {
        /* Event watches may run on the posting thread; copy before dispatch. */
        NSString *text = [NSString stringWithUTF8String:event->drop.data];
        if ([text hasPrefix:@"discord-1565271043529117697:"])
            dispatch_async(dispatch_get_main_queue(), ^{ receive_callback([NSURL URLWithString:text]); });
    }
    return true;
}
#if !HALO_NATIVE_MAC
@interface VISRDiscordPresentation : NSObject <ASWebAuthenticationPresentationContextProviding>
@end
@implementation VISRDiscordPresentation
- (ASPresentationAnchor)presentationAnchorForWebAuthenticationSession:(ASWebAuthenticationSession *)session {
    (void)session;
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes)
        if ([scene isKindOfClass:UIWindowScene.class])
            for (UIWindow *window in ((UIWindowScene *)scene).windows) if (window.isKeyWindow) return window;
    return nil;
}
@end
static VISRDiscordPresentation *presentation;
#endif
static void link_discord(void) {
    if (!client) return;
    ++generation;
    refreshing = false; presence_pending = false; refresh_retry = 0; refresh_attempts = 0;
    [authentication cancel]; authentication = nil;
    auto pkce = client->CreateAuthorizationCodeVerifier();
    verifier = @(pkce.Verifier().c_str());
    pending_state = host_discord_random_state();
    NSURL *url = host_discord_authorization_url(verifier, pending_state);
    if (!url) { status_text = @"Could not start secure Discord sign-in."; return; }
    authorization_started = NSProcessInfo.processInfo.systemUptime;
    status_text = @"Waiting for Discord authorization…";
    host_logf(HOST_LOG_INFO, "Discord: PKCE authorization requested with custom-scheme callback");
#if HALO_NATIVE_MAC
    if ([NSWorkspace.sharedWorkspace openURL:url]) host_logf(HOST_LOG_INFO, "Discord: authorization browser opened");
    else { pending_state = nil; verifier = nil; status_text = @"Could not open the authorization browser."; }
#else
    unsigned request = generation;
    authentication = [[ASWebAuthenticationSession alloc] initWithURL:url
        callbackURLScheme:@"discord-1565271043529117697" completionHandler:^(NSURL *callback, NSError *error) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (request != generation) return;
                if (callback) receive_callback(callback);
                else { pending_state = nil; verifier = nil; status_text = @"Discord sign-in canceled."; }
                (void)error;
                authentication = nil;
            });
        }];
    if (!presentation) presentation = [VISRDiscordPresentation new];
    authentication.presentationContextProvider = presentation;
    if (![authentication start]) { pending_state = nil; verifier = nil; status_text = @"Could not open Discord sign-in."; }
#endif
}
static void unlink_discord(void) {
    ++generation;
    pending_state = nil; verifier = nil;
    [authentication cancel]; authentication = nil;
    linked = false; refreshing = false; presence_pending = false; published_valid = false;
    refresh_retry = 0; refresh_attempts = 0;
    OSStatus read;
    NSDictionary *tokens = host_discord_tokens_read(&read);
    if (client) {
        client->ClearRichPresence();
        if (tokens) client->RevokeToken(application_id, [tokens[@"access"] UTF8String], [](discordpp::ClientResult result) {
            report_result("revoke", result);
        });
        client->Disconnect();
    }
    OSStatus deleted = host_discord_tokens_delete();
    status_text = deleted == errSecSuccess ? @"Not linked" : @"Could not remove the Discord link from Keychain.";
    host_logf(HOST_LOG_INFO, "Discord: unlink requested; Keychain delete status %d", (int)deleted);
}
void host_discord_shutdown(void) {
    ++generation;
    [callback_timer invalidate]; callback_timer = nil;
    [authentication cancel]; authentication = nil;
    pending_state = nil; verifier = nil;
    SDL_RemoveEventWatch(discord_url, NULL);
    if (client) { client->ClearRichPresence(); client->Disconnect(); client.reset(); }
}
void host_discord_install(void) {
    if (client) return;
    client = std::make_unique<discordpp::Client>();
    client->SetApplicationId(application_id);
    client->SetStatusChangedCallback([](discordpp::Client::Status status, discordpp::Client::Error error, int32_t detail) {
        host_logf(HOST_LOG_INFO, "Discord: status %d, error %d, detail %d", (int)status, (int)error, detail);
        if (status == discordpp::Client::Status::Ready && linked) {
            status_text = @"Linked"; published_valid = false; publish_presence();
        } else if (status == discordpp::Client::Status::Disconnected && linked)
            status_text = @"Linked; disconnected from Discord";
    });
    client->SetTokenExpirationCallback([] { refresh_tokens(); });
    SDL_AddEventWatch(discord_url, NULL);
    callback_timer = [NSTimer timerWithTimeInterval:0.1 repeats:YES block:^(NSTimer *timer) {
        (void)timer;
        discordpp::RunCallbacks();
#if !HALO_NATIVE_MAC
        host_discord_ui_install();
#endif
        if (refresh_retry && NSProcessInfo.processInfo.systemUptime >= refresh_retry) {
            refresh_retry = 0;
            refresh_tokens();
        }
        if (pending_state.length && NSProcessInfo.processInfo.systemUptime - authorization_started > 300) {
            pending_state = nil; verifier = nil; status_text = @"Discord sign-in timed out. Try again.";
            [authentication cancel]; authentication = nil;
        }
        static NSTimeInterval last;
        NSTimeInterval now = NSProcessInfo.processInfo.systemUptime;
        if (now - last >= 15) { last = now; publish_presence(); }
    }];
    [NSRunLoop.mainRunLoop addTimer:callback_timer forMode:NSRunLoopCommonModes];
#if HALO_NATIVE_MAC
    [NSNotificationCenter.defaultCenter addObserverForName:NSApplicationWillTerminateNotification object:nil
        queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) { (void)note; host_discord_shutdown(); }];
#endif
    host_logf(HOST_LOG_INFO, "Discord: SDK 1.10.19337 initialized; authenticated presence only");
    refresh_tokens();
}
void host_discord_show_settings(void) {
    const bool was_linked = linked;
    NSString *message = [NSString stringWithFormat:@"%@. Linking shares your map, game mode, and player counts with Discord.", status_text];
#if HALO_NATIVE_MAC
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Discord"; alert.informativeText = message;
    [alert addButtonWithTitle:was_linked ? @"Unlink Discord" : @"Link Discord"];
    [alert addButtonWithTitle:@"Done"];
    if (pending_state.length) [alert addButtonWithTitle:@"Cancel sign-in"];
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) { if (was_linked) unlink_discord(); else link_discord(); }
    else if (response == NSAlertThirdButtonReturn) unlink_discord();
#else
    UIWindow *window = [presentation presentationAnchorForWebAuthenticationSession:nil];
    if (!window) {
        if (!presentation) presentation = [VISRDiscordPresentation new];
        window = [presentation presentationAnchorForWebAuthenticationSession:nil];
    }
    UIViewController *controller = window.rootViewController;
    while (controller.presentedViewController) controller = controller.presentedViewController;
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Discord" message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:was_linked ? @"Unlink Discord" : @"Link Discord" style:UIAlertActionStyleDefault
        handler:^(UIAlertAction *action) { (void)action; if (was_linked) unlink_discord(); else link_discord(); }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Done" style:UIAlertActionStyleCancel handler:nil]];
    [controller presentViewController:alert animated:YES completion:nil];
#endif
}
#if HALO_NATIVE_MAC
@interface VISRDiscordSettings : NSObject
- (void)show:(id)sender;
@end
@implementation VISRDiscordSettings
- (void)show:(id)sender { (void)sender; host_discord_show_settings(); }
@end
NSMenuItem *host_discord_settings_item(void) {
    static VISRDiscordSettings *settings;
    if (!settings) settings = [VISRDiscordSettings new];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Discord Settings…" action:@selector(show:) keyEquivalent:@""];
    item.target = settings;
    return item;
}
#endif
