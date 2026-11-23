/* Hands the audio session back when the last sound device closes, telling other apps they may resume (a paused music
   player starts again). SDL deactivates the session when its last device closes but without that notice. macOS has no
   audio session. Never called while a device is open: nothing else may touch the session then. */
#import <TargetConditionals.h>
#include <stdio.h>
#include "host_audio_session.h"

#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_VISION
#import <AVFoundation/AVFoundation.h>
#include <stdatomic.h>

/* Music off: other apps' audio plays at full volume. SDL's session ducks other audio, and its category hint only offers
   playback or ambient (ambient obeys the silent switch), so this one piece is re-applied after SDL opens a device:
   Playback with MixWithOthers and no ducking. */
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

void host_audio_session_describe(char *out,size_t size)
{
    AVAudioSession *session=AVAudioSession.sharedInstance;
    NSMutableArray<NSString *> *types=[NSMutableArray array];
    for(AVAudioSessionPortDescription *port in session.currentRoute.outputs)
        [types addObject:port.portType];
    snprintf(out,size,"io_buffer=%.2f ms preferred_io_buffer=%.2f ms output_latency=%.2f ms rate=%.0f Hz channels=%ld ports=[%s]",
        session.IOBufferDuration*1000.0,session.preferredIOBufferDuration*1000.0,session.outputLatency*1000.0,
        session.sampleRate,(long)session.outputNumberOfChannels,[types componentsJoinedByString:@","].UTF8String);
}

void host_audio_session_release(void)
{
    [AVAudioSession.sharedInstance setActive:NO
                                 withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                       error:nil];
}
#elif TARGET_OS_OSX
#include <CoreAudio/CoreAudio.h>
static int hal_u32(AudioObjectID object,AudioObjectPropertyScope scope,AudioObjectPropertySelector selector,UInt32 *value)
{
    AudioObjectPropertyAddress address={selector,scope,kAudioObjectPropertyElementMain};
    UInt32 size=sizeof(*value);
    return AudioObjectGetPropertyData(object,&address,0,NULL,&size,value)==noErr;
}
void host_audio_session_set_mix_only(int enabled)
{
    (void)enabled;
}

void host_audio_session_apply(void)
{
}

/* macOS has no audio session: the system default output device's own numbers (frames, at its nominal rate), not SDL's
   queue (its size is "sample frames" on the audio device line) and not the device SDL opened if that is not the default. */
void host_audio_session_describe(char *out,size_t size)
{
    AudioObjectID device=kAudioObjectUnknown;
    UInt32 length=sizeof(device);
    AudioObjectPropertyAddress address={kAudioHardwarePropertyDefaultOutputDevice,kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject,&address,0,NULL,&length,&device)!=noErr || device==kAudioObjectUnknown) {
        snprintf(out,size,"no default output device");
        return;
    }
    UInt32 buffer=0,latency=0,safety=0,stream_latency=0,transport=0;
    Float64 rate=0;
    int have_buffer=hal_u32(device,kAudioObjectPropertyScopeGlobal,kAudioDevicePropertyBufferFrameSize,&buffer);
    int have_latency=hal_u32(device,kAudioObjectPropertyScopeOutput,kAudioDevicePropertyLatency,&latency);
    int have_safety=hal_u32(device,kAudioObjectPropertyScopeOutput,kAudioDevicePropertySafetyOffset,&safety);
    int have_transport=hal_u32(device,kAudioObjectPropertyScopeGlobal,kAudioDevicePropertyTransportType,&transport);
    address=(AudioObjectPropertyAddress){kAudioDevicePropertyNominalSampleRate,kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    length=sizeof(rate);
    if(AudioObjectGetPropertyData(device,&address,0,NULL,&length,&rate)!=noErr)rate=0;
    /* the first output stream's latency */
    int have_stream=0;
    address=(AudioObjectPropertyAddress){kAudioDevicePropertyStreams,kAudioObjectPropertyScopeOutput,kAudioObjectPropertyElementMain};
    AudioObjectID streams[8];
    length=sizeof(streams);
    if(AudioObjectGetPropertyData(device,&address,0,NULL,&length,streams)==noErr && length>=sizeof(AudioObjectID))
        have_stream=hal_u32(streams[0],kAudioObjectPropertyScopeGlobal,kAudioStreamPropertyLatency,&stream_latency);
    /* -1: the property was not readable */
    snprintf(out,size,"buffer_frames=%d device_latency=%d safety_offset=%d stream_latency=%d rate=%.0f Hz transport=%d",
        have_buffer?(int)buffer:-1,have_latency?(int)latency:-1,have_safety?(int)safety:-1,have_stream?(int)stream_latency:-1,
        rate,have_transport?(int)transport:-1);
}

void host_audio_session_release(void)
{
}
#else
void host_audio_session_describe(char *out,size_t size)
{
    snprintf(out,size,"no audio session");
}

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
