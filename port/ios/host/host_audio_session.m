/* Hands the audio session back when the last sound device closes, telling other apps they may resume (a paused music
   player starts again). SDL deactivates the session when its last device closes but without that notice. macOS has no
   audio session. Never called while a device is open: nothing else may touch the session then. */
#import <TargetConditionals.h>
#include "host_audio_session.h"

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_VISION
#import <AVFoundation/AVFoundation.h>

void host_audio_session_release(void)
{
    [AVAudioSession.sharedInstance setActive:NO
                                 withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                       error:nil];
}
#else
void host_audio_session_release(void)
{
}
#endif
