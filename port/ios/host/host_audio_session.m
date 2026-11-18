/* Hands the audio session back when the last sound device closes, telling other apps they may resume (a paused music
   player starts again). SDL deactivates the session when its last device closes but without that notice. macOS has no
   audio session. Never called while a device is open: nothing else may touch the session then. */
#import <TargetConditionals.h>
#include "host_audio_session.h"

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_VISION
#import <AVFoundation/AVFoundation.h>
#include <stdatomic.h>

/* Music off: other apps' audio plays at full volume. SDL's session ducks other audio, and its category hint only offers
   playback or ambient (ambient obeys the silent switch), so this one piece is re-applied after SDL opens a device:
   Playback with MixWithOthers and no DuckOthers. */
static atomic_int mix_only;

void host_audio_session_set_mix_only(int enabled)
{
    atomic_store(&mix_only, enabled != 0);
}

void host_audio_session_apply(void)
{
    if(!atomic_load(&mix_only))return;
    AVAudioSession *session=AVAudioSession.sharedInstance;
    [session setCategory:AVAudioSessionCategoryPlayback
             withOptions:AVAudioSessionCategoryOptionMixWithOthers
                   error:nil];
    [session setActive:YES error:nil];
}

void host_audio_session_release(void)
{
    [AVAudioSession.sharedInstance setActive:NO
                                 withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                       error:nil];
}
#else
void host_audio_session_set_mix_only(int enabled)
{
    (void)enabled;
}

void host_audio_session_apply(void)
{
}

void host_audio_session_release(void)
{
}
#endif
